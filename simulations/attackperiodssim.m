%% Hashrate attack number of adjustment periods simulation
%% Author: Sebastian Rusu <sebastian.rusu@upandrunningsoftware.com>
%% Created: 2019-02-26
%% Copyright (C) 2019 ObEN, Inc.

% Evolution of the number of adjustment periods with the hash rate
%
% n = floor(1+log(dr, hattack/hinit)
%
% where,
%
% dr = drmax^sign(hattack-hinit)
%
% assuming that hattack != hinit.

clear all; close all;

%%%%%%%%%%%%%%%%%%%%%
% model configuration

drmax = 4;                        % maximum ratio for the adjustment of the difficulty
global hinit = 335 * 10^12;       % initial hashrate (H/s)
hr = 10^3;                        % attack hashrate ratio (1 - no attack)
npts = 10^3;                      % number of points in the simulation

figure(1);

%%%%%%%%%%%%%%%%%%%%%
% Influence of bigger hash rates on the number of periods for settling

h = hinit+1 : (hr*hinit-(hinit+1)) / npts : hr*hinit;
dr = drmax;
n = [];

for i = 1:length(h),
  n(i) = floor(1 + log(h(i)/hinit) / log(dr));
endfor

subplot(1, 2, 1);
plot(h / hinit, n);
hold on;
title("Influence of bigger hash rates on the number of periods for settling");
xlabel("Attack relative hashrate");
ylabel("Number of periods");
grid on;
hold off;

%%%%%%%%%%%%%%%%%%%%%
% Influence of lower hash rates on the number of periods for settling

h = (1/hr)*hinit : ((hinit-1)-(1/hr)*hinit) / npts : hinit-1;
dr = 1/drmax;
n = [];

for i = 1:length(h),
  n(i) = floor(1 + log(h(i)/hinit) / log(dr));
endfor

subplot(1, 2, 2);
plot(h / hinit, n);
hold on;
title("Influence of lower hash rates on the number of periods for settling");
xlabel("Attack relative hashrate");
ylabel("Number of periods");
grid on;
hold off;

figure(2);

%%%%%%%%%%%%%%%%%%%%%
% Influence of bigger hash rates and difficulty adjustment ratio on the number of periods for settling

h = hinit+1 : (hr*hinit-(hinit+1)) / npts : hr*hinit;
dr = 4: 1 :100;
n = [];

for i = 1:length(h),
  for j = 1:length(dr),
    n(i,j) = floor(1 + log(h(i)/hinit) / log(dr(j)));
  endfor
endfor

subplot(1, 2, 1);
mesh(dr, h / hinit, n);
hold on;
title("Influence of bigger hash rates and difficulty adjustment ratio on the number of periods for settling");
xlabel("Difficulty adjustment ratio");
ylabel("Attack relative hashrate");
zlabel("Number of periods");
grid on;
hold off;

%%%%%%%%%%%%%%%%%%%%%
% Influence of lower hash rates and difficulty adjustment ratio on the number of periods for settling

h = (1/hr)*hinit : ((hinit-1)-(1/hr)*hinit) / npts : hinit-1;
dr = 1/100 : (1/4 - 1/100) / npts : 1/4;
n = [];

for i = 1:length(h),
  for j = 1:length(dr),
    n(i,j) = floor(1 + log(h(i)/hinit) / log(dr(j)));
  endfor
endfor

subplot(1, 2, 2);
mesh(dr, h / hinit, n);
hold on;
title("Influence of lower hash rates and difficulty adjustment ratio on the number of periods for settling");
xlabel("Difficulty adjustment ratio");
ylabel("Attack relative hashrate");
zlabel("Number of periods");
grid on;
hold off;
