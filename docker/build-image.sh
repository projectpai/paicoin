#!/usr/bin/env bash

GIT_REPO=github.com
PAICOIN_BRANCH="hybrid-consensus"

for i in "$@"
do
case $i in
    -b=*|--branch=*)
    PAICOIN_BRANCH="${i#*=}"
    shift
    ;;
    -g=*|--gitrepo=*)
    GIT_REPO="${i#*=}"
    shift
    ;;
    -v=*|--version=*)
    VERSION="${i#*=}"
    shift
    ;;
    *)

    ;;
esac
done

echo "VERSION     = ${VERSION:?No version tag. Please specify it using this option: --version=<v>}"

TAG="$PAICOIN_BRANCH:$VERSION"
echo "TAG         = $TAG"
DOCKER_FILE="$PAICOIN_BRANCH/Dockerfile"
echo "DOCKER_FILE = $DOCKER_FILE"
echo "GIT_REPO    = $GIT_REPO"
echo "PAI BRANCH  = $PAICOIN_BRANCH"

docker build --rm --build-arg PAICOIN_BRANCH=$PAICOIN_BRANCH --build-arg CONTAINER_VERSION=$VERSION --build-arg GIT_REPO=$GIT_REPO --tag $TAG -f $DOCKER_FILE .
