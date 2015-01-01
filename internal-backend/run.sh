case $1 in

  init)
    ;;

  build)
    http-server/gyp/gyp --depth=. -Ihttp-server/libuv/common.gypi -Dtarget_arch=x86_64 -Dlibrary=static_library api.gyp -f make
    make
    ;;

  build_mac)
    http-server/gyp/gyp --depth=. -Ihttp-server/libuv/common.gypi -Dtarget_arch=x86_64 -Dlibrary=static_library api.gyp -f xcode
    # xcodebuild -verbose -project uhunt_api.xcodeproj -configuration Release -target api
    xcodebuild -verbose -project uhunt_api.xcodeproj -configuration Debug -target api
    ;;

  api)
    ./run.sh build && ./out/Debug/api -P 8000 -U 8001 -S $@
    ;;

  backup)
    cp tps.bin backup/tps$2.bin
    cp crowd.bin backup/crowd$2.bin
    cp kesalahan.bin backup/kesalahan$2.bin
    cp hal3.bin backup/hal3$2.bin
    ;;

  set_main)
    cp backup/tps$2.bin tps.bin
    cp backup/crowd$2.bin crowd.bin
    cp backup/kesalahan$2.bin kesalahan.bin
    cp backup/hal3$2.bin hal3.bin
    ;;

  rmc1)
    rm c1/htmls/$2.html
    ;;

  copy_newer)
    find thumb/ -name "*.jpg" -newer thumb/000122200104.jpg -print | xargs zip thumbs11.zip
    ;;

  chmod_thumb)
    find c1/thumb/ -name "*.jpg" | xargs chmod 644
    ;;

  clean)
    rm -rf out build api.xcodeproj
    ;;

  mv)
    find $2 -type f -name '*' -exec mv {} $3/. \;
    ;;

esac
