pkg load control
pkg load signal

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

# Faster bode plot directly computed from impulse response fft.
function fft_bode(irs)
  ir = irs(:,1:1);

  global samplerate;
  N = length(ir);
  fft1 = fft(ir);
  ampl   = abs(fft1);
  phase  = angle(fft1) * 180 / pi;
  f_step = samplerate / N
  
  # Limit the frequency range
  f_left = 20;
  # f_right = 20000;
  f_right = samplerate / 2
  offset_start  = 1 + round(f_left  / f_step);
  offset_end    = 1 + round(f_right / f_step);


  # Don't include DC and NY
  # offset_start = 2
  # offset_end   = N/2 - 1

  f_0 = f_step * (offset_start - 1);
  

  db = 20 * log10(ampl(offset_start:offset_end));
  ph = phase(offset_start:offset_end);


  nb_f = length(db);
  x = linspace(f_0, f_step * (nb_f-1), nb_f);

  subplot (2, 1, 1)
  semilogx(x,db);
  xlim([f_left f_right]);
  ylim([-80 20])
  grid ("on");
  ylabel ("Magnitude [dB]");
  xlabel ("Frequency [Hz]");

  subplot (2, 1, 2)
  semilogx(x,ph);
  xlim([f_left f_right]);
  ylim([-180 180])
  yticks([-180 -90 0 90 180])
  grid ("on");
  ylabel ("Phase [rad]");
  xlabel ("Frequency [Hz]");
  

  # plot(x,f1)
end

# With delay compensation
function fft_bode_dly(ir, dly)
  ir_shift = circshift(ir', -dly)';
  fft_bode(ir_shift);
end



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
