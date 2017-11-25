#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

#include <libnova/libnova.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <math.h>
#include <vector>
#include <algorithm>
#include <string>
#include <map>
#include <syslog.h>

using std::vector;
using std::pair;
using std::string;
using std::ofstream;

struct ln_lnlat_posn observer{-91.634659, 42.042523};

void getSolarCoords(double JD, ln_hrz_posn* pos) {
    struct ln_equ_posn equ;
    ln_get_solar_equ_coords(JD, &equ);
    ln_get_hrz_from_equ(&equ, &observer, JD, pos);
    if (pos->az > 180) pos->az -= 360;
}

double getAzimuth(double JD) {
    struct ln_hrz_posn pos;
    getSolarCoords(JD, &pos);
    return pos.az;
}

double getAltitude(double JD) {
    struct ln_hrz_posn pos;
    getSolarCoords(JD, &pos);
    return pos.alt;
}

void printTime(std::ofstream& fout, const double JD) {
    ln_date date;
    ln_get_date(JD, &date);
    date.seconds = round(date.seconds);
    if (date.seconds == 60) { date.seconds = 0; date.minutes += 1; }
    if (date.minutes == 60) { date.minutes = 0; date.hours += 1; }
    if (date.hours == 24) { date.days += 1; date.hours = 0; }
    fout << std::fixed << std::setfill('0')
         << std::setw(2) << date.months << '/'
         << std::setw(2) << date.days << '/'
         << std::setw(4) << date.years << ' '
         << std::setw(2) << date.hours << ':'
         << std::setw(2) << date.minutes << ':'
         << std::setw(2) << int(round(date.seconds));
}

using photo_t = pair<string, double>;
using schedule_t = vector<photo_t>;

schedule_t schedule;

int schedule_day{ 0 };
const string schedulePath{ "schedule.txt" };

const int photos_per_day{ 50 };
const long cet_offset = -6 * 60 * 60;
struct ln_zonedate boy { 2018, 1, 1, 0, 0, 0, cet_offset };
struct ln_zonedate eoy { 2019, 1, 1, 0, 0, 0, cet_offset };
const double boyJD = ln_get_julian_local_date(&boy);
const double eoyJD = ln_get_julian_local_date(&eoy);
const int fixedHour = 13;
const string fixedTag{ "FIXED" };
const string sunriseTag{ "SUNRISE" };
const string sunsetTag{ "SUNSET" };
const string transitTag{ "TRANSIT" };
const string yiadTag{ "YIAD" };

size_t schedule_index{ 0 };

double getMidnight() {
    const double baseJD = ln_get_julian_from_sys();
    struct ln_date baseDate;
    ln_get_date(baseJD, &baseDate);
    struct ln_zonedate midnight {
        baseDate.years, baseDate.months, baseDate.days, 0, 0, 0, cet_offset
    };
    return ln_get_julian_local_date(&midnight);
}

void calculateSchedule() {
    const double baseJD = getMidnight();
    struct ln_date baseDate;
    ln_get_date(baseJD, &baseDate);
    if (schedule.size() > 0 && schedule_day == baseDate.days) return;
    schedule_day = baseDate.days;
    schedule.clear(); schedule_index = 0;

    struct ln_zonedate fixedDate;
    fixedDate.years   = baseDate.years;
    fixedDate.months  = baseDate.months;
    fixedDate.days    = baseDate.days;
    fixedDate.hours   = fixedHour;
    fixedDate.minutes = 0;
    fixedDate.seconds = 0;
    fixedDate.gmtoff  = cet_offset;
    const double fixedJD = ln_get_julian_local_date(&fixedDate);
    schedule.emplace_back(fixedTag, fixedJD);

    struct ln_rst_time rst;
    ln_get_solar_rst(baseJD, &observer, &rst);
    const double one_min = 1.0d / (60 * 24);

    const double sunrise = rst.rise;    
    schedule.emplace_back(sunriseTag, sunrise);
    schedule.emplace_back(sunriseTag, sunrise + one_min);
    const double sunset = rst.set;
    schedule.emplace_back(sunsetTag, sunset - one_min);
    schedule.emplace_back(sunsetTag, sunset);
    const double transit = rst.transit;
    schedule.emplace_back(transitTag, transit);
    
    const double interval = (sunset - sunrise) / (photos_per_day + 1);
    const string tagPrefix{ "PER_" };
    for (int i = 1; i <= photos_per_day; i++) {
        const string tag = tagPrefix + std::to_string(i);
        schedule.emplace_back(tag, sunrise + i * interval);
    }

    const double fraction = (transit - boyJD) / (eoyJD - boyJD);
    const double YIAD_JD = sunrise + (sunset - sunrise) * fraction;
    schedule.emplace_back(yiadTag, YIAD_JD);

    auto photoComp = [](const photo_t& lhs, const photo_t& rhs) -> bool {
        return lhs.second < rhs.second;
    };
    std::sort(schedule.begin(), schedule.end(), photoComp);

    ofstream fout{ schedulePath, ofstream::trunc };
    for (const auto& photo : schedule) {
        fout << photo.first << ' ';
        printTime(fout, photo.second);
        fout << std::endl;
    }
}

const string takePhotoScript{ "/home/pi/take_photo.sh " };
void takePhoto() {
    if (schedule_index >= schedule.size()) return;
    const photo_t& photo = schedule[schedule_index];
    const double JD = ln_get_julian_from_sys();
    if (JD < photo.second) return;
    string command{ takePhotoScript + photo.first };
    system(command.c_str());
    schedule_index++;
}

void setup() {
    pid_t pid, sid;
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);
    
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    if (chdir("/home/pi/solar_tracker") < 0) {
        exit(EXIT_FAILURE);
    }
    
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(int argc, char* argv[]) {
    setup();
    while (true) {
        try {
            calculateSchedule();
            takePhoto();
        } catch (...) {}
        sleep(1);
    }
    exit(EXIT_SUCCESS);
}