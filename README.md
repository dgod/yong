# 小小输入法

[讨论区](http://yong.dgod.net/)

小小输入法在 Linux 下开发和编译。没有测试过在其他操作系统下进行编译。
需要是64位系统，32位系统某些模块就没法编译，需要自己手工调整。
`gcc` 编译器，需要支持c99以上版本。clang应该也行。编译 Windows 版本需要 mingw-w64 工具链（Windows版暫時閉源）。
`nodejs` 需要0.12版本及以上。
系统中需要安装好相关的开发环境，比如gtk3-devel，gtk2-devel，ibus-devel，libxkbcommon-devel。
准备好 [build.js](https://github.com/dgod/build.js)

Ubuntu 下編譯：
- 安装基本编译工具 `apt install build-essential git`
- 下载源码 `git clone https://github.com/dgod/yong.git`
- 安装 lib `apt install libgtk-3-dev libgtk2.0-dev libxkbcommon-dev libibus-1.0-dev libc6-dev-amd64`
- 安装 [Node.js] 0.12 以上版本 (https://nodejs.org/en/download/package-manager/)
- 下载 `build.js`: `wget https://raw.githubusercontent.com/dgod/build.js/master/build.js`
- 由于`git`不能存储空目录，所以下载到的源代码缺少一些目录结构，编译前需要先创建好一些目录
```sh
mkdir -p {llib,cloud,gbk,mb,vim}/{l32,l64}
mkdir -p {im,config}/{l32-gtk3,l32-gtk2,l64-gtk3,l64-gtk2}
mkdir -p im/gtk-im/{l32-gtk3,l32-gtk2,l64-gtk3,l64-gtk2}
mkdir -p im/IMdkit/{l32,l64} 
```
- 进行编译 `nodejs "path/to/build.js" l32 l64`
