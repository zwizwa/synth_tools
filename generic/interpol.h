#ifndef INTERPOL_H
#define INTERPOL_H


/* Compute Lagrange FIR coefficients from interpolation fraction.

   The purpose here is not to make constant interpolator, and not to
   make an interpolator where the fraction is updated every sample, so
   we can take the fraction and compute the FIR convolution mask for a
   given order.  The filters are made symmetrical, which means that
   for even order the 0 point is in the middle of the 2 middle taps,
   and for odd order it is the middle tap. */

typedef float coef_t;
static inline void lagrange_get_fir_even(coef_t *fir, int order, coef_t x) {
    /* For even number of taps we pick the x=0 point in the middle
       between the two middle samples.  The sample points then are:

       ..., -3/2, -1/2, 1/2, 3/2, ...

    */
    int half_order = order / 2;
    for (int j=0; j<order; j++) {
        /* Follow definitions from
           https://en.wikipedia.org/wiki/Polynomial_interpolation */
        coef_t num = 1.0;
        int        den = 1;

        coef_t xj = 0.5 + j-half_order;
        for (int i=0; i<order; i++) {
            coef_t xi = 0.5 + i-half_order;
            if (j != i) {
                num *= (x-xi);
                den *= (j-i);  // simplified from xj-xi
            }
        }
        fir[j] = num / ((coef_t)den);
    }
}


#endif
