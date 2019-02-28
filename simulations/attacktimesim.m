## Hashrate attack duration simulation
## Author: Sebastian Rusu <sebastian.rusu@upandrunningsoftware.com>
## Created: 2019-02-26
## Copyright (C) 2019 ObEN, Inc.

# Settling time evolution on hashrate changes
#
#      hinit * tideal * np * (1 - dr^n)
# T = ----------------------------------
#            hattack * (1 - dr)
#
# where,
#
# dr = drmax^sign(hattack-hinit)
#
# assuming that hattack != hinit.

clear all; close all;

#####################
# model configuration

tideal = 10 * 60;                 # ideal block time (s)
np = 2016;                        # number of blocks in an adjustment interval
drmax = 4;                        # maximum ratio for the adjustment of the difficulty
global hinit = 335 * 10^12;       # initial hashrate (H/s)
hr = 100;                         # attack hashrate ratio (1 - no attack)
npts = 100;                       # number of points in the simulation
dinit = hinit * (tideal / 2^32);  # initial difficulty
global npsim = 1:50:2016;         # adjustment interval settings

figure(1);

#####################
# Settling time under majority attacks with bigger hashrates (standard algorithm)

h = hinit+1 : (hr*hinit-(hinit+1)) / npts : hr*hinit;
d = h * (tideal / (2^32));
dr = drmax;
T = [];

for i = 1:length(h),
    n = gpexp(dinit, dr, d(i));
    if n != -1,
      T(i) = (hinit * tideal * np * (1 - dr^n)) / (h(i) * (1-dr));
    else
      T(i) = T(i-1);
    endif
endfor

subplot(1,2,1);
plot(h / hinit, T / (60*60));
hold on;
title("Settling time under majority attacks with bigger hashrates (standard algorithm)");
xlabel("Attack relative hashrate");
ylabel("Time (hours)");
grid on;
hold off;

#####################
# Settling time under majority attacks with lower hashrates (standard algorithm)

h = (1/hr)*hinit : ((hinit-1)-(1/hr)*hinit) / npts : hinit-1;
d = h * (tideal / (2^32));
dr = 1/drmax;
T = [];

for i = 1:length(h),
    n = gpexp(dinit, dr, d(i));
    if n != -1,
      T(i) = (hinit * tideal * np * (1 - dr^n)) / (h(i) * (1-dr));
    else
      T(i) = T(i-1);
    endif
endfor

subplot(1,2,2);
plot(h / hinit, T / (60*60));
hold on;
title("Settling time under majority attacks with lower hashrates (standard algorithm)");
xlabel("Attack relative hashrate");
ylabel("Time (hours)");
grid on;
hold off;

figure(2);

#####################
# Difficulty adjustment interval influence on the settling time in majority attacks

h = hr*hinit;
d = h * (tideal / (2^32));
dr = drmax;
n = gpexp(dinit, dr, d);
T = [];

for i = 1:length(npsim),
  T(i) = (hinit * tideal * npsim(i) * (1 - dr^n)) / (h * (1-dr));
endfor

subplot(1,2,1);
plot(npsim, T / (60*60));
hold on;
title("Difficulty adjustment interval influence on the settling time in majority attacks");
xlabel("No. of blocks in adjustment interval");
ylabel("Time (hours)");
grid on;
hold off;

#####################
# Difficulty adjustment ratio influence on the settling time in majority attacks

h = hr*hinit;
d = h * (tideal / (2^32));
drmaxmax = 50;
dr = drmax : (drmaxmax-1) * drmax / npts : drmaxmax*drmax;
T = [];

for i = 1:length(dr),
  n = gpexp(dinit, dr(i), d);
  T(i) = (hinit * tideal * np * (1 - dr(i)^n)) / (h * (1-dr(i)));
endfor

subplot(1,2,2);
plot(dr, T / (60*60));
hold on;
title("Difficulty adjustment ratio influence on the settling time in majority attacks");
xlabel("Adjustment ratio");
ylabel("Time (hours)");
grid on;
hold off;

figure(3);

#####################
# Difficulty adjustment interval influence on the settling time in majority attacks with bigger hashrates

h = hinit+1 : (hr*hinit-(hinit+1)) / npts : hr*hinit;
d = h * (tideal / (2^32));
dr = drmax;
npsim = 1:50:2016;
T = [];

for i = 1:length(h),
    n = gpexp(dinit, dr, d(i));
    
    for j = 1:length(npsim),
      if n != -1,
        T(i,j) = (hinit * tideal * npsim(j) * (1 - dr^n)) / (h(i) * (1-dr));
      else
       T(i,j) = T(i,j-1);
      endif
    endfor
endfor

subplot(1,2,1);
mesh(npsim, h / hinit, T / (60*60));
hold on;
title("Difficulty adjustment interval influence on the settling time in majority attacks with bigger hashrates");
xlabel("No. of blocks in adjustment interval");
ylabel("Attack relative hashrate");
zlabel("Time (hours)");
grid on;
hold off;

#####################
# Difficulty adjustment interval influence on the settling time in majority attacks with lower hashrates

h = (1/hr)*hinit : ((hinit-1)-(1/hr)*hinit) / npts : hinit-1;
d = h * (tideal / (2^32));
dr = 1/drmax;
npsim = 1:50:2016;
T = [];

for i = 1:length(h),
    n = gpexp(dinit, dr, d(i));
    
    for j = 1:length(npsim),
      if n != -1,
        T(i,j) = (hinit * tideal * npsim(j) * (1 - dr^n)) / (h(i) * (1-dr));
      else
       T(i,j) = T(i,j-1);
      endif
    endfor
endfor

subplot(1,2,2);
mesh(npsim, h / hinit, T / (60*60));
hold on;
title("Difficulty adjustment interval influence on the settling time in majority attacks with lower hashrates");
xlabel("No. of blocks in adjustment interval");
ylabel("Attack relative hashrate");
zlabel("Time (hours)");
grid on;
hold off;

figure(4);

#####################
# Difficulty adjustment ratio influence on the settling time in majority attacks with bigger hashrates

h = hinit+1 : (hr*hinit-(hinit+1)) / npts : hr*hinit;
d = h * (tideal / (2^32));
drmaxmax = 50;
dr = drmax : (drmaxmax-1) * drmax / npts : drmaxmax*drmax;
T = [];

for i = 1:length(h),
    for j = 1:length(dr),
      n = gpexp(dinit, dr(j), d(i));
      if n != -1,
        T(i,j) = (hinit * tideal * np * (1 - dr(j)^n)) / (h(i) * (1-dr(j)));
      else
        T(i,j) = T(i,j-1);
      endif
    endfor
endfor

subplot(1,2,1);
mesh(h / hinit, dr, T / (60*60));
hold on;
title("Difficulty adjustment ratio influence on the settling time in majority attacks with bigger hashrates");
xlabel("Adjustment ratio");
ylabel("Attack relative hashrate");
zlabel("Time (hours)");
grid on;
hold off;

#####################
# Difficulty adjustment ratio influence on the settling time in majority attacks with lower hashrates

h = (1/hr)*hinit : ((hinit-1)-(1/hr)*hinit) / npts : hinit-1;
d = h * (tideal / (2^32));
drmaxmax = 50;
dr = 1/(drmaxmax*drmax) : (1/drmax-1/(drmaxmax*drmax)) / npts : 1/drmax ;
T = [];

for i = 1:length(h),
    for j = 1:length(dr),
      n = gpexp(dinit, dr(j), d(i));
      if n != -1,
        T(i,j) = (hinit * tideal * np * (1 - dr(j)^n)) / (h(i) * (1-dr(j)));
      else
        T(i,j) = T(i,j-1);
      endif
    endfor
endfor

subplot(1,2,2);
mesh(h / hinit, dr, T / (60*60));
hold on;
title("Difficulty adjustment interval influence on the settling time in majority attacks with lower hashrates");
xlabel("Adjustment ratio");
ylabel("Attack relative hashrate");
zlabel("Time (hours)");
grid on;
hold off;

figure(5);

#####################
# Settling time dependence on adjustment interval and ratio under majority attack with bigger hashrate

global hdyn = (1/hr)*hinit : ((hr*hinit)-(1/hr)*hinit) / npts : hr*hinit;
ddyn = hdyn * (tideal / (2^32));
drmaxmax = 50;
global npsim = 1:50:2016;
global Tdyn = [];
global drdyn = [];

for k=1:length(hdyn),
  if hdyn(k) >= hinit,
    drdyn(k,1:npts+1) = drmax : (drmaxmax-1) * drmax / npts : drmaxmax*drmax;
  else
    drdyn(k,1:npts+1) = 1/(drmaxmax*drmax) : (1/drmax-1/(drmaxmax*drmax)) / npts : 1/drmax;
  endif
  
  for i = 1:length(npsim),
      for j = 1:length(drdyn(k,:)),
        n = gpexp(dinit, drdyn(k,j), ddyn(k));
        if n != -1,
          Tdyn(k,i,j) = (hinit * tideal * npsim(i) * (1 - drdyn(k,j)^n)) / (hdyn(k) * (1-drdyn(k,j)));
        else
          Tdyn(k,i,j) = Tdyn(k,i,j-1);
        endif
      endfor
  endfor
endfor

mesh(drdyn(1,:), npsim, reshape(Tdyn(1,:,:), [size(Tdyn)(2) ,size(Tdyn)(3)]) / (60*60));
hold on;
title(sprintf("Settling time dependence on adjustment interval and ratio under majority attack with relative hashrate %f", hdyn(k) / hinit));
xlabel("Adjustment ratio");
ylabel("No. of blocks in adjustment interval");
zlabel("Time (hours)");
grid on;
hold off;

# Redraw settling time dependence on difficulty adjustment interval and ratio under a different attack hashrate
function redrawstvsaiar(h, event)
  global hinit;
  global hdyn;
  global npsim
  global drdyn;
  global Tdyn;

  hrset = get (h, 'value');
  
  k = 1;
  while ((k < length(hdyn)) && (hrset * hinit > hdyn(k))),
    k = k + 1;
  endwhile
  if k == length(hdyn),
    return;
  endif

  figure(5);
  mesh(drdyn(k,:), npsim, reshape(Tdyn(k,:,:), [size(Tdyn)(2), size(Tdyn)(3)]) / (60*60));
  hold on;
  title(sprintf("Settling time dependence on adjustment interval and ratio under majority attack with relative hashrate %f", hrset));
  xlabel("Adjustment ratio");
  ylabel("No. of blocks in adjustment interval");
  zlabel("Time (hours)");
  grid on;
  hold off;
end

hslider = uicontrol (
  'style', 'slider',
  'Units', 'normalized',
  'position', [0.1, 0.05, 0.8, 0.1],
  'min', (1/hr),
  'max', hr,
  'value', 1,
  'callback', {@redrawstvsaiar}
);
