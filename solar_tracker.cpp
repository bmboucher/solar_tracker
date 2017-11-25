#include <libnova/libnova.h>
#include <iostream>
#include <iomanip>
#include <fstream>

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

int main(int argc, char* argv[]) {
    struct ln_zonedate boy;
    boy.years = 2018;
    boy.months = 1;
    boy.days = 1;
    boy.hours = 0;
    boy.minutes = 0;
    boy.seconds = 0;
    boy.gmtoff = -6 * 60 * 60;

    struct ln_zonedate eoy;
    eoy.years = 2018;
    eoy.months = 12;
    eoy.days = 31;
    eoy.hours = 23;
    eoy.minutes = 59;
    eoy.seconds = 59;
    eoy.gmtoff = -6 * 60 * 60;

    double JD = ln_get_julian_local_date(&boy);
    std::cout << std::fixed;
    std::cout << "Starting Julian date = " << std::setprecision(6) << JD << std::endl;
    const double endJD = ln_get_julian_local_date(&eoy);
    std::cout << "Ending Julian date = " << std::setprecision(6) << endJD << std::endl;

    return 0;
}
