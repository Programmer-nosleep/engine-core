#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H

typedef struct AtmosphereState
{
  float cloud_time_seconds;
  float time_of_day;
  float sun_direction[3];
} AtmosphereState;

#endif
