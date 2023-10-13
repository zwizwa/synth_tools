/*  Combine putch detection and granual synthesis / convolution.

    Idea is to take the input signal and distort it to square/pulse
    wave, then isolate the dicontinuities and map them to
    grain-triggering or a proper convolution with the differential
    (positive and negative diract pulses).

    Then in a second pass, use the average (pulse density) to modify
    the timbre of the grain.  E.g. stretch it to match pitch.

    This should give a near perfect pitch mapping, and smooth out the
    timbre mapping over a longer period of time because it is assumed
    to be not so important.

    To make this work the time resolution should probably be very
    high, i.e. much more than 44.1kHz

    Convolution is likely overkill, but a properly sampled grain is
    probably a good idea.

    How to test?

    Start from a bass lick.

    Perform prefiltering and interpolated zero crossing to emulate
    high resolution analog preprocessing step.

    Then implement the grain playback at normal sample frequency with
    interpolation.


    Find a platform first.  It seems this is best structured as a Pd
    object.

*/

int main(int argc, char **argv) {
    return 0;
}
