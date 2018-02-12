#!/bin/sh

TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
SRCDIR=${SRCDIR:-$TOPDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

PAICOIND=${PAICOIND:-$SRCDIR/paicoind}
PAICOINCLI=${PAICOINCLI:-$SRCDIR/paicoin-cli}
PAICOINTX=${PAICOINTX:-$SRCDIR/paicoin-tx}
PAICOINQT=${PAICOINQT:-$SRCDIR/qt/paicoin-qt}

[ ! -x $PAICOIND ] && echo "$PAICOIND not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
PAIVER=($($PAICOINCLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }'))

# Create a footer file with copyright content.
# This gets autodetected fine for paicoind if --version-string is not set,
# but has different outcomes for paicoin-qt and paicoin-cli.
echo "[COPYRIGHT]" > footer.h2m
$PAICOIND --version | sed -n '1!p' >> footer.h2m

for cmd in $PAICOIND $PAICOINCLI $PAICOINTX $PAICOINQT; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${PAIVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${PAIVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m
