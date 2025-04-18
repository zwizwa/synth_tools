pkg load control
# pkg load signal

# Check closed code that depend on this before changing API!

## -*- octave -*-
function abode(f)
  bode(f, {20,24000});
end

global samplerate = 48000

function f = fir(ir)
  global samplerate;
  f = filt(ir, [1], 1/samplerate);
end

# Bode plot converting impulse response to discrete system.
function bode_fir(ir)
  f = fir(ir);
  abode(f);
end

global z = tf('z', 1/samplerate);



function [db, ph, f_0, f_step] = fft_to_spectrum(fft1)

  global samplerate;
  N = length(fft1);
  ampl   = abs(fft1);
  phase  = angle(fft1) * 180 / pi;
  f_step = samplerate / N;
  
  # Limit the frequency range
  f_left = 20;
  # f_right = 20000;
  f_right = samplerate / 2;
  offset_start  = 1 + round(f_left  / f_step);
  offset_end    = 1 + round(f_right / f_step);

  # Don't include DC and NY
  # offset_start = 2
  # offset_end   = N/2 - 1

  f_0 = f_step * (offset_start - 1);
  
  db = 20 * log10(ampl(offset_start:offset_end));
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
  ylim([-80 20])
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
function fft_bode(irs)
  [db, ph, f_0, f_step] = fft_spectrum(irs, 1);
  if size(irs)(2) == 2
    [db1, ph1] = fft_spectrum(irs, 2);
    ph = mod(ph-ph1+180, 360)-180;
  end
  subplot_db(2, 1, 1, f_0, f_step, db)
  subplot_ph(2, 1, 2, f_0, f_step, ph)
end


# With delay compensation.  We are computing the spectrum of a
# periodic signal so rotation is appropriate here.
function fft_bode_dly(ir, dly)
  ir_shift = circshift(ir', -dly)';
  fft_bode(ir_shift);
end


# Matrix transfer plot.



# Vector of 0,T,2T,... T=1/samplerate
function t = time(n)
  global samplerate
  T = 1/samplerate;
  t = linspace(0,(n-1)*T,n);
end

function sig = sinsr(f,n);
  t = time(n);
  sig = sin(2*pi*f*t);
end
