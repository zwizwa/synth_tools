# -*- octave -*-
pkg load control
# pkg load signal

# Check closed code that depend on this before changing API!

## -*- octave -*-
function abode(f)
  bode(f, {20,24000});
end

function sr = samplerate()
  sr = 48000;
end

function f = fir(ir)
  f = filt(ir, [1], 1/samplerate());
end

# Bode plot converting impulse response to discrete system.
function bode_fir(ir)
  f = fir(ir);
  abode(f);
end

# FIXME: global variables don't seem to reload, so use functions instead.
# global z = tf('z', 1/samplerate());


# Unwrap phase. The heuristic is to track the phase increments and
# pick which ever jump is smallest: p, p+2pi, p-2pi and track the
# number of revolutions.

# span should be 2*pi if the input is radians, or 360 if degrees.
function unwrapped = unwrap_phase(wrapped, span)
  wraps = 0;
  unwrapped = [];
  last = wrapped(1);
  for i=1:length(wrapped)
    dl = wrapped(i) - last;
    d  = abs(dl);
    dm = abs(dl - span);
    dp = abs(dl + span);
    if dp < d && dp < dm
      wraps = wraps + 1;
    elseif dm < d && dm < dp
      wraps = wraps - 1;
    end
    uw = wrapped(i) + wraps * span;
    unwrapped(i) = uw;
    last = wrapped(i);
  end
end

# Compute group delay
#
# The group delay is defined as -dp/dw where p is the phase in radians
# and w is the angular frequency 2*pi*f/f_s in radians per normalized
# time unit which is 1 sample.
#
# The group delay is then expressed in fractional samples.
#
# The approximation uses difference with the previous FFT bin: (p_{i}
# - p_{i-1}) / (w_{i} - (w_{i-1})
#
# The w difference is one FFT bin which is 2*pi / N
#
function [gd, f_0, f_step] = group_delay(fft1)
  N = length(fft1);
  phase_r = unwrap_phase(angle(fft1),2*pi); %% In radians, unwrapped
  scale = N / (2*pi);
  gd1 = conv(scale*[-1 1], phase_r);
  gd = gd1(2:(N/2));
  f_step = samplerate() / N;
  f_0 = f_step; %% frequency of the first sample
end


function [db, ph, f_0, f_step] = fft_to_spectrum(fft1)

  N = length(fft1);
  ampl   = abs(fft1);
  phase  = angle(fft1) * 180 / pi;
  f_step = samplerate() / N;
  
  # Limit the frequency range
  f_left = 20;
  # f_right = 20000;
  f_right = samplerate() / 2;
  offset_start  = 1 + round(f_left  / f_step);
  offset_end    = 1 + round(f_right / f_step);

  # Don't include DC and NY
  # offset_start = 2
  # offset_end   = N/2 - 1

  f_0 = f_step * (offset_start - 1);
  
  db = 20 * log10(ampl(offset_start:offset_end));
  # Unwrapped phase display isn't very useful in a bode plot.  Make
  # separate phas end group/phase delay plots for that.
  # ph = unwrap_phase(phase(offset_start:offset_end), 360);
  ph = phase(offset_start:offset_end);

end

function [db, ph, f_0, f_step] = fft_spectrum(irs, c)
  ir = irs(:,c:c);
  fft1 = fft(ir);
  [db, ph, f_0, f_step] = fft_to_spectrum(fft1);
end


function [x,f_left,f_right] = bode_x(f_0, f_step, y)
  nb_f = length(y);
  x = linspace(f_0, f_step * (nb_f-1), nb_f);
  f_left  = f_0;
  f_right = f_0 + (nb_f - 1) * f_step;
end

# https://www.mathworks.com/help/matlab/ref/subplot.html
# m x n is rows x columns
# p is plot number (col1,row1 then col2,row1 etc)
function subplot_db(rows, cols, plot_nb, f_0, f_step, db)
  [x,f_left,f_right] = bode_x(f_0, f_step, db);
  subplot (rows, cols, plot_nb)
  semilogx(x,db);
  xlim([f_left f_right]);
  ylim([-120 20])
  grid ("on");
  ylabel ("Magnitude [dB]");
  xlabel ("Frequency [Hz]");
end

function subplot_ph(rows, cols, plot_nb, f_0, f_step, ph)
  [x,f_left,f_right] = bode_x(f_0, f_step, ph);
  subplot (rows, cols, plot_nb)
  semilogx(x,ph);
  xlim([f_left f_right]);
  ylim([-180 180])
  yticks([-180 -90 0 90 180])
  grid ("on");
  ylabel ("Phase [rad]");
  xlabel ("Frequency [Hz]");
end


# Faster bode plot directly computed from impulse response fft.
# Note that using fft we approximate by computing the spectrum of a
# periodic signal.  As long as the impulse is long enough (= padded
# enough), the effect of the periodicity is minimal.
#
# As long as impulse response is "padded enough", the apprixmation
# is reasonable.  I've been using 8K samples (32 x 256).

# Split up into:
# - compute spectrum via fft
# - optionally compute phase difference if two columns are given
# - plot db magnitude and phase

function fft_bode_fft1(fft1)
  [db, ph, f_0, f_step] = fft_to_spectrum(fft1);
  subplot_db(2, 1, 1, f_0, f_step, db)
  subplot_ph(2, 1, 2, f_0, f_step, ph)
end

function fft_bode_trans(sig_in, sig_out)
  fft1 = fft(sig_out) ./ fft(sig_in);
  fft_bode_fft1(fft1)
end
function fft_bode(ir)
  fft_bode_fft1(fft(ir))
end

# Plot group delay
function plot_group_delay(ir)
  fft1 = fft(ir);
  [gd, f_0, f_step] = group_delay(fft1);
  [x,f_left,f_right] = bode_x(f_0, f_step, gd);
  f_left
  f_right
  subplot(1, 1, 1);
  semilogx(x,gd);
end
function plot_unwrapped_phase(ir)
  fft1 = fft(ir);
  [db, ph, f_0, f_step] = fft_to_spectrum(fft1);
  phu = unwrap_phase(ph, 360);
  [x,f_left,f_right] = bode_x(f_0, f_step, phu);
  subplot(1, 1, 1);
  plot(x,phu);
end



# Originally for plotting relative phase of iir hilbert transformer
# with frequency dependent i->o group delay, but 90 between outputs.
function fft_phase_diff(ir, ir1)
  [db,  ph,  f_0,  f_step]  = fft_to_spectrum(fft(ir));
  [db1, ph1, f_01, f_step1] = fft_to_spectrum(fft(ir1));
  ph = mod(ph-ph1+180, 360)-180;
  subplot_ph(1, 1, 1, f_0, f_step, ph);
end



# With delay compensation.  We are computing the spectrum of a
# periodic signal so rotation is appropriate here.
function fft_bode_dly(ir, dly)
  ir_shift = circshift(ir', -dly)';
  fft_bode(ir_shift);
end


# Vector of 0,T,2T,... T=1/samplerate()
function t = time(n)
  T = 1/samplerate();
  t = linspace(0,(n-1)*T,n);
end

function sig = sinsr(f,n);
  t = time(n);
  sig = sin(2*pi*f*t);
end


