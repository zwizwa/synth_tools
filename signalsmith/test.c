#include "dsp/delay.h"

// https://signalsmith-audio.co.uk/code/dsp/html/structsignalsmith_1_1delay_1_1_interpolator_kaiser_sinc_n.html
// https://en.wikipedia.org/wiki/Kaiser_window

void sinc20(void) {
    signalsmith::delay::InterpolatorKaiserSinc20<float> interpolator;
    float buf[] = {1,2,3,4,5,6,7,8};
    interpolator.write(0, buf[0]);
}

int main(int argc, char **argv) {
    sinc20();
    return 0;
}
