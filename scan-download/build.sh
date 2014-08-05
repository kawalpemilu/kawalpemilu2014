#!/bin/bash

BASE=$(cd `dirname $0`; pwd)
cd $BASE
export GOPATH=$BASE
go build download

