# Detect ARM64 Linux
ARCH_FLAGS=""
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    case $(uname -m) in
        arm)     dpkg --print-architecture | grep -q "arm64" && ARCH_FLAGS="--build=aarch64-unknown-linux-gnu" || ARCH_FLAGS="" ;;
        aarch64) ARCH_FLAGS="--build=aarch64-unknown-linux-gnu" ;;
    esac
fi

PAICOIN_ROOT=$(pwd)

# Pick some path to install BDB to, here we create a directory within the paicoin directory
BDB_PREFIX="${PAICOIN_ROOT}/db4"
mkdir -p $BDB_PREFIX

# Fetch the source and verify that it is not tampered with
wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
echo '12edc0df75bf9abd7f82f821795bcee50f42cb2e5f76a6a281b85732798364ef  db-4.8.30.NC.tar.gz' | sha256sum -c
# -> db-4.8.30.NC.tar.gz: OK
tar -xzvf db-4.8.30.NC.tar.gz

# Apply atomic name conflict patch
cp -f depends/patches/db4/atomic.h db-4.8.30.NC/dbinc/atomic.h

# Build the library and install to our prefix
cd db-4.8.30.NC/build_unix/
#  Note: Do a static build so that it can be embedded into the executable, instead of having to find a .so at runtime
../dist/configure --enable-cxx --disable-shared --with-pic --prefix=$BDB_PREFIX $ARCH_FLAGS
make install

# Configure PAIcoin Core to use our own-built instance of BDB
cd $PAICOIN_ROOT
./autogen.sh
./configure LDFLAGS="-L${BDB_PREFIX}/lib/" CPPFLAGS="-I${BDB_PREFIX}/include/" $ARCH_FLAGS # (other args...)