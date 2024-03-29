# Copyright (c) 2018-2020 The Project PAI Foundation
# Copyright (c) 2013-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

DIST_SUBDIRS = secp256k1 univalue

AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS) $(HARDENED_LDFLAGS)
AM_CXXFLAGS = $(HARDENED_CXXFLAGS) $(ERROR_CXXFLAGS)
AM_CPPFLAGS = $(HARDENED_CPPFLAGS)
EXTRA_LIBRARIES =

if EMBEDDED_UNIVALUE
LIBUNIVALUE = univalue/libunivalue.la

$(LIBUNIVALUE): $(wildcard univalue/lib/*) $(wildcard univalue/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)
else
LIBUNIVALUE = $(UNIVALUE_LIBS)
endif

PAICOIN_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BDB_CPPFLAGS) $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS)

PAICOIN_INCLUDES += -I$(srcdir)/secp256k1/include
PAICOIN_INCLUDES += $(UNIVALUE_CFLAGS)

LIBPAICOIN_SERVER=libpaicoin_server.a
LIBPAICOIN_COMMON=libpaicoin_common.a
LIBPAICOIN_CONSENSUS=libpaicoin_consensus.a
LIBPAICOIN_CLI=libpaicoin_cli.a
LIBPAICOIN_UTIL=libpaicoin_util.a
LIBPAICOIN_CRYPTO=crypto/libpaicoin_crypto.a
LIBPAICOINQT=qt/libpaicoinqt.a
LIBSECP256K1=secp256k1/libsecp256k1.la

if TARGET_WINDOWS
LIBPAICOINHASH=-L$(srcdir)/crypto/paicoinhash/win -lpaicoinhash
else
if TARGET_DARWIN
LIBPAICOINHASH=-L$(srcdir)/crypto/paicoinhash/macos -lpaicoinhash
else
LIBPAICOINHASH= -L$(srcdir)/crypto/paicoinhash/linux -lpaicoinhash
endif
endif

if ENABLE_ZMQ
LIBPAICOIN_ZMQ=libpaicoin_zmq.a
endif
if ENABLE_WALLET
LIBPAICOIN_WALLET=libpaicoin_wallet.a
endif

$(LIBSECP256K1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES += \
  $(LIBPAICOIN_CRYPTO) \
  $(LIBPAICOIN_UTIL) \
  $(LIBPAICOIN_COMMON) \
  $(LIBPAICOIN_CONSENSUS) \
  $(LIBPAICOIN_SERVER) \
  $(LIBPAICOIN_CLI) \
  $(LIBPAICOIN_WALLET) \
  $(LIBPAICOIN_ZMQ)

bin_PROGRAMS =
noinst_PROGRAMS =
TESTS =
BENCHMARKS =

if BUILD_PAICOIND
  bin_PROGRAMS += paicoind
endif

if BUILD_PAICOIN_UTILS
  bin_PROGRAMS += paicoin-cli paicoin-block-generator paicoin-tx
endif

.PHONY: FORCE check-symbols check-security
# paicoin core #
PAICOIN_CORE_H = \
  addrdb.h \
  addrman.h \
  base58.h \
  bloom.h \
  blockencodings.h \
  chain.h \
  chainparams.h \
  chainparamsbase.h \
  chainparamsseeds.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coins.h \
  coinbase_addresses.h \
  compat.h \
  compat/byteswap.h \
  compat/endian.h \
  compat/sanity.h \
  compressor.h \
  consensus/consensus.h \
  consensus/tx_verify.h \
  core_io.h \
  core_memusage.h \
  cuckoocache.h \
  fs.h \
  httprpc.h \
  httpserver.h \
  indirectmap.h \
  init.h \
  key.h \
  key_io.h \
  keystore.h \
  dbwrapper.h \
  limitedmap.h \
  memusage.h \
  merkleblock.h \
  miner.h \
  net.h \
  net_processing.h \
  netaddress.h \
  netbase.h \
  netmessagemaker.h \
  noui.h \
  policy/feerate.h \
  policy/fees.h \
  policy/policy.h \
  policy/rbf.h \
  pow.h \
  protocol.h \
  random.h \
  reverse_iterator.h \
  reverselock.h \
  rpc/blockchain.h \
  rpc/client.h \
  rpc/mining.h \
  rpc/protocol.h \
  rpc/safemode.h \
  rpc/server.h \
  rpc/register.h \
  scheduler.h \
  script/sigcache.h \
  script/sign.h \
  script/standard.h \
  script/ismine.h \
  stake/stakepoolfee.h \
  stake/stakeversion.h \
  streams.h \
  support/allocators/secure.h \
  support/allocators/zeroafterfree.h \
  support/cleanse.h \
  support/events.h \
  support/lockedpool.h \
  sync.h \
  threadsafety.h \
  threadinterrupt.h \
  timedata.h \
  torcontrol.h \
  txdb.h \
  txmempool.h \
  ui_interface.h \
  undo.h \
  util.h \
  utilmoneystr.h \
  utiltime.h \
  validation.h \
  validationinterface.h \
  versionbits.h \
  wallet/coincontrol.h \
  wallet/crypter.h \
  wallet/db.h \
  wallet/feebumper.h \
  wallet/fees.h \
  wallet/init.h \
  wallet/rpcwallet.h \
  wallet/wallet.h \
  wallet/walletdb.h \
  warnings.h \
  zmq/zmqabstractnotifier.h \
  zmq/zmqconfig.h\
  zmq/zmqnotificationinterface.h \
  zmq/zmqpublishnotifier.h


obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh "$(abs_top_builddir)/src/obj/build.h" \
	  "$(abs_top_srcdir)"
libpaicoin_util_a-clientversion.$(OBJEXT): obj/build.h

# server: shared between paicoind and paicoin-qt
libpaicoin_server_a_CPPFLAGS = $(AM_CPPFLAGS) $(PAICOIN_INCLUDES) $(MINIUPNPC_CPPFLAGS) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS)
libpaicoin_server_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libpaicoin_server_a_SOURCES = \
  addrdb.cpp \
  addrman.cpp \
  bloom.cpp \
  blockencodings.cpp \
  checkpoints.cpp \
  consensus/tx_verify.cpp \
  httprpc.cpp \
  httpserver.cpp \
  init.cpp \
  dbwrapper.cpp \
  merkleblock.cpp \
  miner.cpp \
  net.cpp \
  net_processing.cpp \
  noui.cpp \
  policy/fees.cpp \
  policy/policy.cpp \
  policy/rbf.cpp \
  pow.cpp \
  rest.cpp \
  rpc/blockchain.cpp \
  rpc/mining.cpp \
  rpc/misc.cpp \
  rpc/net.cpp \
  rpc/rawtransaction.cpp \
  rpc/safemode.cpp \
  rpc/server.cpp \
  rpc/stakeapi.cpp \
  script/sigcache.cpp \
  script/ismine.cpp \
  stake/stakeversion.cpp \
  timedata.cpp \
  torcontrol.cpp \
  txdb.cpp \
  txmempool.cpp \
  ui_interface.cpp \
  validation.cpp \
  validationinterface.cpp \
  versionbits.cpp \
  $(PAICOIN_CORE_H)

if ENABLE_ZMQ
libpaicoin_zmq_a_CPPFLAGS = $(PAICOIN_INCLUDES) $(ZMQ_CFLAGS)
libpaicoin_zmq_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libpaicoin_zmq_a_SOURCES = \
  zmq/zmqabstractnotifier.cpp \
  zmq/zmqnotificationinterface.cpp \
  zmq/zmqpublishnotifier.cpp
endif


# wallet: shared between paicoind and paicoin-qt, but only linked
# when wallet enabled
libpaicoin_wallet_a_CPPFLAGS = $(AM_CPPFLAGS) $(PAICOIN_INCLUDES)
libpaicoin_wallet_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libpaicoin_wallet_a_SOURCES = \
  wallet/crypter.cpp \
  wallet/db.cpp \
  wallet/feebumper.cpp \
  wallet/fees.cpp \
  wallet/init.cpp \
  wallet/rpcdump.cpp \
  wallet/rpcwallet.cpp \
  wallet/auto-revoker/autorevokerconfig.cpp \
  wallet/auto-revoker/autorevoker.cpp \
  wallet/auto-voter/autovoterconfig.cpp \
  wallet/auto-voter/autovoter.cpp \
  wallet/ticket-buyer/ticketbuyerconfig.cpp \
  wallet/ticket-buyer/ticketbuyer.cpp \
  wallet/wallet.cpp \
  wallet/walletdb.cpp \
  stake/stakepoolfee.cpp \
  $(PAICOIN_CORE_H)

# crypto primitives library
crypto_libpaicoin_crypto_a_CFLAGS = $(AM_CFLAGS) $(PIC_FLAGS)
crypto_libpaicoin_crypto_a_CPPFLAGS = $(AM_CPPFLAGS)
crypto_libpaicoin_crypto_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libpaicoin_crypto_a_SOURCES = \
  crypto/aes.cpp \
  crypto/aes.h \
  crypto/chacha20.h \
  crypto/chacha20.cpp \
  crypto/common.h \
  crypto/hmac_sha256.cpp \
  crypto/hmac_sha256.h \
  crypto/hmac_sha512.cpp \
  crypto/hmac_sha512.h \
  crypto/ripemd160.cpp \
  crypto/ripemd160.h \
  crypto/sha1.cpp \
  crypto/sha1.h \
  crypto/sha256.cpp \
  crypto/sha256.h \
  crypto/sha512.cpp \
  crypto/sha512.h \
  crypto/tiny_sha3.c \
  crypto/tiny_sha3.h

if USE_ASM
crypto_libpaicoin_crypto_a_SOURCES += crypto/sha256_sse4.cpp
endif

# consensus: shared between all executables that validate any consensus rules.
libpaicoin_consensus_a_CPPFLAGS = $(AM_CPPFLAGS) $(PAICOIN_INCLUDES)
libpaicoin_consensus_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libpaicoin_consensus_a_SOURCES = \
  amount.h \
  arith_uint256.cpp \
  arith_uint256.h \
  chain.cpp \
  chainparams.cpp \
  chainparamsbase.cpp \
  consensus/merkle.cpp \
  consensus/merkle.h \
  consensus/params.h \
  consensus/validation.h \
  hash.cpp \
  hash.h \
  prevector.h \
  primitives/block.cpp \
  primitives/block.h \
  primitives/transaction.cpp \
  primitives/transaction.h \
  pubkey.cpp \
  pubkey.h \
  script/paicoinconsensus.cpp \
  script/interpreter.cpp \
  script/interpreter.h \
  script/script.cpp \
  script/script.h \
  script/script_error.cpp \
  script/script_error.h \
  script/standard.cpp \
  serialize.h \
  stake/extendedvotebits.cpp \
  stake/extendedvotebits.h \
  stake/hash256prng.cpp \
  stake/hash256prng.h \
  stake/hasher.cpp \
  stake/hasher.h \
  stake/treap/value.cpp \
  stake/treap/value.h \
  stake/treap/treapnode.cpp \
  stake/treap/treapnode.h \
  stake/treap/tickettreap.cpp \
  stake/treap/tickettreap.h \
  stake/stakenode.cpp \
  stake/stakenode.h \
  stake/staketx.cpp \
  stake/staketx.h \
  stake/votebits.cpp \
  stake/votebits.h \
  tinyformat.h \
  uint256.cpp \
  uint256.h \
  utilstrencodings.cpp \
  utilstrencodings.h \
  version.h
libpaicoin_consensus_a_LDFLAGS = $(LIBPAICOINHASH)

# common: shared between paicoind, and paicoin-qt and non-server tools
libpaicoin_common_a_CPPFLAGS = $(AM_CPPFLAGS) $(PAICOIN_INCLUDES)
libpaicoin_common_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libpaicoin_common_a_SOURCES = \
  base58.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  core_read.cpp \
  core_write.cpp \
  key.cpp \
  key_io.cpp \
  keystore.cpp \
  netaddress.cpp \
  netbase.cpp \
  policy/feerate.cpp \
  protocol.cpp \
  scheduler.cpp \
  script/sign.cpp \
  script/standard.cpp \
  stake/stakepoolfee.cpp \
  warnings.cpp \
  $(PAICOIN_CORE_H)

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libpaicoin_util_a_CPPFLAGS = $(AM_CPPFLAGS) $(PAICOIN_INCLUDES)
libpaicoin_util_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libpaicoin_util_a_SOURCES = \
  support/lockedpool.cpp \
  chainparamsbase.cpp \
  clientversion.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  compat/strnlen.cpp \
  fs.cpp \
  random.cpp \
  rpc/protocol.cpp \
  support/cleanse.cpp \
  sync.cpp \
  threadinterrupt.cpp \
  util.cpp \
  utilmoneystr.cpp \
  utilstrencodings.cpp \
  utiltime.cpp \
  $(PAICOIN_CORE_H)

if GLIBC_BACK_COMPAT
libpaicoin_util_a_SOURCES += compat/glibc_compat.cpp
endif

# cli: shared between paicoin-cli, paicoin-block-generator and paicoin-qt
libpaicoin_cli_a_CPPFLAGS = $(AM_CPPFLAGS) $(PAICOIN_INCLUDES)
libpaicoin_cli_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libpaicoin_cli_a_SOURCES = \
  rpc/client.cpp \
  $(PAICOIN_CORE_H)

nodist_libpaicoin_util_a_SOURCES = $(srcdir)/obj/build.h
#

# paicoind binary #
paicoind_SOURCES = paicoind.cpp
paicoind_CPPFLAGS = $(AM_CPPFLAGS) $(PAICOIN_INCLUDES)
paicoind_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
paicoind_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
paicoind_SOURCES += paicoind-res.rc
endif

paicoind_LDADD = \
  $(LIBPAICOIN_SERVER) \
  $(LIBPAICOIN_COMMON) \
  $(LIBUNIVALUE) \
  $(LIBPAICOIN_UTIL) \
  $(LIBPAICOIN_WALLET) \
  $(LIBPAICOIN_ZMQ) \
  $(LIBPAICOIN_CONSENSUS) \
  $(LIBPAICOINHASH) \
  $(LIBPAICOIN_CRYPTO) \
  $(LIBLEVELDB) \
  $(LIBLEVELDB_SSE42) \
  $(LIBMEMENV) \
  $(LIBSECP256K1)

paicoind_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS) $(EVENT_PTHREADS_LIBS) $(EVENT_LIBS) $(ZMQ_LIBS)

# paicoin-cli binary #
paicoin_cli_SOURCES = paicoin-cli.cpp
paicoin_cli_CPPFLAGS = $(AM_CPPFLAGS) $(PAICOIN_INCLUDES) $(EVENT_CFLAGS)
paicoin_cli_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
paicoin_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
paicoin_cli_SOURCES += paicoin-cli-res.rc
endif

paicoin_cli_LDADD = \
  $(LIBPAICOIN_CLI) \
  $(LIBUNIVALUE) \
  $(LIBPAICOIN_UTIL) \
  $(LIBPAICOINHASH) \
  $(LIBPAICOIN_CRYPTO)

paicoin_cli_LDADD += $(BOOST_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(EVENT_LIBS)
#

# paicoin-block-generator binary #
paicoin_block_generator_SOURCES = paicoin-block-generator.cpp
paicoin_block_generator_CPPFLAGS = $(AM_CPPFLAGS) $(PAICOIN_INCLUDES) $(EVENT_CFLAGS)
paicoin_block_generator_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
paicoin_block_generator_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
paicoin_block_generator_SOURCES += paicoin-block-generator-res.rc
endif

paicoin_block_generator_LDADD = \
  $(LIBPAICOIN_CLI) \
  $(LIBUNIVALUE) \
  $(LIBPAICOIN_UTIL) \
  $(LIBPAICOINHASH) \
  $(LIBPAICOIN_CRYPTO)

paicoin_block_generator_LDADD += $(BOOST_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(EVENT_LIBS)
#

# paicoin-tx binary #
paicoin_tx_SOURCES = paicoin-tx.cpp
paicoin_tx_CPPFLAGS = $(AM_CPPFLAGS) $(PAICOIN_INCLUDES)
paicoin_tx_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
paicoin_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
paicoin_tx_SOURCES += paicoin-tx-res.rc
endif

paicoin_tx_LDADD = \
  $(LIBUNIVALUE) \
  $(LIBPAICOIN_COMMON) \
  $(LIBPAICOIN_UTIL) \
  $(LIBPAICOIN_CONSENSUS) \
  $(LIBPAICOINHASH) \
  $(LIBPAICOIN_CRYPTO) \
  $(LIBSECP256K1)

paicoin_tx_LDADD += $(BOOST_LIBS) $(CRYPTO_LIBS)
#

CTAES_DIST =  crypto/ctaes/bench.c
CTAES_DIST += crypto/ctaes/ctaes.c
CTAES_DIST += crypto/ctaes/ctaes.h
CTAES_DIST += crypto/ctaes/README.md
CTAES_DIST += crypto/ctaes/test.c

CLEANFILES = $(EXTRA_LIBRARIES)

CLEANFILES += *.gcda *.gcno
CLEANFILES += compat/*.gcda compat/*.gcno
CLEANFILES += consensus/*.gcda consensus/*.gcno
CLEANFILES += crypto/*.gcda crypto/*.gcno
CLEANFILES += policy/*.gcda policy/*.gcno
CLEANFILES += primitives/*.gcda primitives/*.gcno
CLEANFILES += script/*.gcda script/*.gcno
CLEANFILES += support/*.gcda support/*.gcno
CLEANFILES += univalue/*.gcda univalue/*.gcno
CLEANFILES += wallet/*.gcda wallet/*.gcno
CLEANFILES += wallet/test/*.gcda wallet/test/*.gcno
CLEANFILES += zmq/*.gcda zmq/*.gcno

DISTCLEANFILES = obj/build.h

EXTRA_DIST = $(CTAES_DIST)


config/paicoin-config.h: config/stamp-h1
	@$(MAKE) -C $(top_builddir) $(subdir)/$(@)
config/stamp-h1: $(top_srcdir)/$(subdir)/config/paicoin-config.h.in $(top_builddir)/config.status
	$(AM_V_at)$(MAKE) -C $(top_builddir) $(subdir)/$(@)
$(top_srcdir)/$(subdir)/config/paicoin-config.h.in:  $(am__configure_deps)
	$(AM_V_at)$(MAKE) -C $(top_srcdir) $(subdir)/config/paicoin-config.h.in

clean-local:
	-$(MAKE) -C secp256k1 clean
	-$(MAKE) -C univalue clean
	-rm -f leveldb/*/*.gcda leveldb/*/*.gcno leveldb/helpers/memenv/*.gcda leveldb/helpers/memenv/*.gcno
	-rm -f config.h
	-rm -rf test/__pycache__

.rc.o:
	@test -f $(WINDRES)
	## FIXME: How to get the appropriate modulename_CPPFLAGS in here?
	$(AM_V_GEN) $(WINDRES) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) -DWINDRES_PREPROC -i $< -o $@

.mm.o:
	$(AM_V_CXX) $(OBJCXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	  $(CPPFLAGS) $(AM_CXXFLAGS) $(QT_INCLUDES) $(AM_CXXFLAGS) $(PIE_FLAGS) $(CXXFLAGS) -c -o $@ $<

check-symbols: $(bin_PROGRAMS)
if GLIBC_BACK_COMPAT
	@echo "Checking glibc back compat..."
	$(AM_V_at) READELF=$(READELF) CPPFILT=$(CPPFILT) $(top_srcdir)/contrib/devtools/symbol-check.py < $(bin_PROGRAMS)
endif

check-security: $(bin_PROGRAMS)
if HARDEN
	@echo "Checking binary security..."
	$(AM_V_at) READELF=$(READELF) OBJDUMP=$(OBJDUMP) $(top_srcdir)/contrib/devtools/security-check.py < $(bin_PROGRAMS)
endif

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(<D) $<

if EMBEDDED_LEVELDB
include Makefile.leveldb.include
endif

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_BENCH
include Makefile.bench.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif
