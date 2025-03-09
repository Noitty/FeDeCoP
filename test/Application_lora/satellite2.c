#include "application.h"

int main()
{
    Satellite sat;
    sat.role = -1;
    sat.available_service = 0;
    sat.type_of_service_available = SERVICE_TYPE_NOT_DEFINED;
    sat.m_service_type_interest = SERVICE_TYPE_DOWNLOAD;
    sat.id_satellite = 2;

    start_osadp(sat);
}