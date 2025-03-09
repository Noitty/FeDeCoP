#include "application.h"

int main()
{
    Satellite sat;
    sat.role = -1;
    sat.available_service = 1;
    sat.type_of_service_available = SERVICE_TYPE_STORAGE;
    sat.m_service_type_interest = SERVICE_TYPE_NOT_DEFINED;
    sat.id_satellite = 1;

    start_osadp(sat);
}