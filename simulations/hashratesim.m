## Hashrate attack simulation
## Author: Sebastian Rusu <sebastian.rusu@upandrunningsoftware.com>
## Created: 2019-02-25
## Copyright (C) 2019 ObEN, Inc.

## In order to simulate a hashrate attack, shape the values as specified in the attack and algorithm settings sections.
## For example, normal attacker + fast algorithm.

clear all; close all;

#####################
# Attack settings:
# Normal attacker: N = 10; cH = [10 * ones(1, n/4+1) 1 * ones(1, n*3/4)];
# Powerfull attacker: N = 30; cH = [100 * ones(1, n/3+1) 1 * ones(1, n*2/3)];
# Huge attacker: N = 100; cH = [1000 * ones(1, n/2+1) 1 * ones(1, n/2)];
#
# Difficulty adjustment algorithm settings:
# Short interval: Np = 144; drmax = 4;
# High adjustment ratio: Np = 2016; drmax = 100;
# Fast: Np = 144; drmax = 100;
# Super fast: Np = 6; drmax = 100;

#####################
# model configuration

N = 10;                                         # number of adjustment periods to simulate
Np = 2016;                                      # number of blocks in an adjustment interval
n = N * Np;                                     # total number of blocks

cH = [10 * ones(1, n/4+1) 1 * ones(1, n*3/4)];  # attack hashrate ratio (1 - no attack)
H = 335 * 1000 * 1000 * 1000 * 1000;            # real hashrate (H/s)

tideal = 10 * 60;                               # ideal block time (s)

d0 = 42000000;                                  # initial difficulty
drmax = 4;                                      # maximum ratio for the adjustment of the difficulty

#####################
# simulation variables

t = [ 0 ];    # block moments (when a block in mined)
d = [ d0 ];   # difficulty values for blocks (correspond to the value of t at the same index)
h = H * cH;   # the instantaneous hashrates
D = tideal * h / 2^32;   # difficulty values for blocks (correspond to the value of t at the same index)

#####################
# iteratively calculate the block times and difficulties

T = 0;
for k = 1:n,
  t(k+1) = t(k) + (2^32 * d(k) / h(k));

  if mod(k+1, Np) == 0,
    d(k+1) = min(d(k)*drmax, max(d(k)/drmax, d(k)*((Np*tideal)/(t(k+1)-T))));    
    T = t(k+1);
  else
    d(k+1) = d(k);
  endif
endfor

#####################
# plots

figure(1);
hold on;
title("Difficulty evolution with time in case of hashrate changes (10x attack, standard algorithm)");
xlabel("Time (hours)");
ylabel("Difficulty");
stairs(t/(60*60), D, 'g');
stairs(t/(60*60), d, 'b');
legend("Real difficulty", "Blockchain difficulty");
grid on;
hold off;