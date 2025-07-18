set -e

CURRENT_PATH=$(cd $(dirname $0) && pwd)
INSTALL_PREFIX="$CURRENT_PATH/../out/install/"
ARCH=$(uname -m)

function build_gfamily() {
  echo "############### Build Google Libs. ################"
  
  # 添加 fmt 库的构建
  pushd "$CURRENT_PATH/.."
  cmake -P "$CURRENT_PATH/../cmake/GetFmt.cmake"
  popd
}

function main() {
  echo "############### Install Third Party. ################"

  build_gfamily

  return
}

main "$@"