#ifndef STARFOX_WATER_TRANSMISSION_INC
#define STARFOX_WATER_TRANSMISSION_INC
#ifndef WATER_OPTICS_FN
#define WATER_OPTICS_FN
#endif
// Beer-Lambert absorption in game-world units. Work in linear light: red
// attenuates faster than green/blue, while shallow water reveals the receiver.
WATER_OPTICS_FN float water_transmittance(float distance,float absorption) {
    return exp(-max(0.f,distance)*max(0.f,absorption));
}
WATER_OPTICS_FN float water_transmitted_channel(float receiver,float body,float distance,float absorption) {
    float transmission=water_transmittance(distance,absorption);
    return receiver*transmission+body*(1.f-transmission);
}
#endif
