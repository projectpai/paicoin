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
hinit = 335 * 10^12;              # initial hashrate (H/s)
hr = 100;                         # attack hashrate ratio (1 - no attack)
npts = 1000;                      # number of points in the simulation
dinit = hinit * (tideal / 2^32);  # initial difficulty

figure(1);

#####################
# Standard algorithm under high hashrates attack

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

subplot(2,4,1);
plot(h / hinit, T / (60*60));
hold on;
title("Settling time against majority attack bigger hashrates");
xlabel("Attack relative hashrate");
ylabel("Time (hours)");
grid on;
hold off;

#####################
# Standard algorithm under low hashrates attack

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

subplot(2,4,5);
plot(h / hinit, T / (60*60));
hold on;
title("Settling time over majority attack lower hashrates");
xlabel("Attack relative hashrate");
ylabel("Time (hours)");
grid on;
hold off;

#####################
# Adjustment interval influence on settling time

h = hr*hinit;
d = h * (tideal / (2^32));
dr = drmax;
n = gpexp(dinit, dr, d);
npsim = 1:2016;
T = [];

for i = 1:length(npsim),
  T(i) = (hinit * tideal * npsim(i) * (1 - dr^n)) / (h * (1-dr));
endfor

subplot(2,4,2);
plot(npsim, T / (60*60));
hold on;
title("Settling time in majority attack against adjustment interval");
xlabel("No. of blocks in adjustment interval");
ylabel("Time (hours)");
grid on;
hold off;

#####################
# Adjustment ratio influence on settling time

h = hr*hinit;
d = h * (tideal / (2^32));
drmaxmax = 50;
dr = drmax : (drmaxmax-1) * drmax / npts : drmaxmax*drmax;
T = [];

for i = 1:length(dr),
  n = gpexp(dinit, dr(i), d);
  T(i) = (hinit * tideal * np * (1 - dr(i)^n)) / (h * (1-dr(i)));
endfor

subplot(2,4,6);
plot(dr, T / (60*60));
hold on;
title("Settling time in majority attack against adjustment ratio");
xlabel("Adjustment ratio");
ylabel("Time (hours)");
grid on;
hold off;

#####################
# Adjustment interval influence on settling time under large hashrate attack

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

subplot(2,4,3);
mesh(npsim, h / hinit, T / (60*60));
hold on;
title("Settling time against adjustment interval under majority attack with bigger hashrates");
xlabel("No. of blocks in adjustment interval");
ylabel("Attack relative hashrate");
zlabel("Time (hours)");
grid on;
hold off;

#####################
# Adjustment interval influence on settling time under low hashrate attack

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

subplot(2,4,7);
mesh(npsim, h / hinit, T / (60*60));
hold on;
title("Settling time against adjustment interval under majority attack with lower hashrates");
xlabel("No. of blocks in adjustment interval");
ylabel("Attack relative hashrate");
zlabel("Time (hours)");
grid on;
hold off;

#####################
# Adjustment ratio influence on settling time under large hashrate attack

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

subplot(2,4,4);
mesh(h / hinit, dr, T / (60*60));
hold on;
title("Settling time against adjustment ratio under majority attack with bigger hashrates");
xlabel("Adjustment ratio");
ylabel("Attack relative hashrate");
zlabel("Time (hours)");
grid on;
hold off;

#####################
# Adjustment ratio influence on settling time under low hashrate attack

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

subplot(2,4,8);
mesh(h / hinit, dr, T / (60*60));
hold on;
title("Settling time against adjustment ratio under majority attack with lower hashrates");
xlabel("Adjustment ratio");
ylabel("Attack relative hashrate");
zlabel("Time (hours)");
grid on;
hold off;
