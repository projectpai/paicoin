%% Get the geometrical progression element exponent closer to a value
%% Author: Sebastian Rusu <sebastian.rusu@upandrunningsoftware.com>
%% Created: 2019-02-26
%% Copyright (C) 2019 ObEN, Inc.

%% Assuming a geometrical progression defined as:
%%
%%   a[n] = a[1] * r^(n-1)
%%
%% find the index n such that the specified b number satisfies:
%%
%%   a[n] <= b < a[n+1], if r > 1, or
%%   a[n] >= b > a[n+1], if r < 1, or
%%

function n = gpexp(a1, r, b)
  n = -1;
  
  if a1 == 0 || r == 1,
    return;
  endif
  
  if r > 1,
    if b == a1,
      n = 1;
    elseif b > a1,
      i = 1;
      while b > a1*(r^(i-1)),
        i = i + 1;
      endwhile
      n = i - 1;
    endif
  elseif r < 1,
    if b == a1,
      n = 1;
    elseif b < a1,
      i = 1;
      while b < a1*(r^(i-1)),
        i = i + 1;
      endwhile
      n = i - 1;
    endif
  endif
endfunction
