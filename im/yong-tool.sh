#!/bin/bash
# yong input method
# author dgod

cd "$(dirname "$0")"

DIST=
CFG=
HOST_ARCH=
HOST_MACHINE=
PATH_LIB32=
PATH_LIB32N=
PATH_LIB64=
HOST_TRIPLET32=
HOST_TRIPLET32N=
HOST_TRIPLET64=
IBUS_STATUS=
GTK2_PATH32=
GTK2_PATH64=
GTK3_PATH32=
GTK3_PATH64=
GTK4_PATH32=
GTK4_PATH64=
GTK2_IMMODULES32=
GTK2_IMMODULES64=
GTK_IM_DETECT=
QT5_PATH32=
QT5_PATH64=
QT6_PATH32=
QT6_PATH64=

function usage()
{
	echo "yong-tool.sh [OPTION]"
	echo "  --install		install yong at system"
	echo "  --install64		install yong64 at system"
	echo "  --install32		install yong32 at system"
	echo "  --uninstall		uninstall yong from system"
	echo "  --select		select yong as default IM"
	echo "  --sysinfo		display system infomation"
}

function fedora_install()
{
	if [ -e /usr/bin/imsettings-info ] ; then
		CFG=/etc/X11/xinit/xinput.d/yong.conf

	cat >$CFG <<EOF
XMODIFIERS="@im=yong"
XIM="yong"
XIM_PROGRAM="/usr/bin/yong"
XIM_ARGS=""
if [ -e $GTK_IM_DETECT/*/immodules/im-yong.so ]; then
	GTK_IM_MODULE="yong"
else
	GTK_IM_MODULE="xim"
fi
if [ -e $QT5_PATH64/plugins/platforminputcontexts/libyongplatforminputcontextplugin.so ]; then
	QT_IM_MODULE="yong"
else
	QT_IM_MODULE="xim"
fi
ICON="`pwd`/skin/tray1.png"
SHORT_DESC="yong"
PREFERENCE_PROGRAM="/usr/bin/yong-config"
EOF

	elif [ -e /etc/X11/xinit/xinputrc ] ; then
		CFG=/etc/X11/xinit/xinput.d/yong.conf

	cat >$CFG <<EOF
XMODIFIERS="@im=yong"
XIM="yong"
XIM_PROGRAM="/usr/bin/yong"
XIM_ARGS=""
XIM_ARGS=""
if [ -e $GTK_IM_DETECT/*/immodules/im-yong.so ] ; then
	GTK_IM_MODULE="yong"
else
	GTK_IM_MODULE="xim"
fi
QT_IM_MODULE="xim"
SHORT_DESC="yong"
PREFERENCE_PROGRAM="/usr/bin/yong-config"
EOF

	else
		CFG=/etc/X11/xinit/xinitrc.d/yong.sh

	cat >$CFG <<EOF
[ -z "$XIM" ] && export XIM=yong
[ -z "$XMODIFIERS" ] && export XMODIFIERS="@im=$XIM"
if [ -z "$GTK_IM_MODULE" ] ; then
	if [ -e $GTK_IM_DETECT/*/immodules/im-yong.so ] ; then
		export GTK_IM_MODULE="yong"
	else
		export GTK_IM_MODULE="xim"
	fi
fi
[ -z "$QT_IM_MODULE" ] && export QT_IM_MODULE=xim
[ -z "$XIM_PROGRAM" ] && export XIM_PROGRAM=yong
[ -z "$XIM_ARGS" ] && export XIM_ARGS="-d"
$XIM_PROGRAM $XIM_ARGS
EOF

		chmod +x $CFG
	fi
}

function fedora_uninstall()
{
	CFG=/etc/X11/xinit/xinput.d/yong.conf
	rm -rf $CFG
	CFG=/etc/X11/xinit/xinitrc.d/yong.sh
	rm -rf $CFG
}

function fedora_select()
{
	CFG=/etc/X11/xinit/xinput.d/yong.conf
	if [ -e $CFG ] ; then
		if [ -e ~/.config/imsettings ] ; then
			ln -sf $CFG ~/.config/imsettings/xinputrc
		else
			ln -sf $CFG ~/.xinputrc
		fi
	fi
}

function debian_install2()
{
	cat >$CFG.conf <<EOF
IM_CONFIG_SHORT="Yong Input Method"
IM_CONFIG_LONG="Yong Input Method"
package_menu () {
	return 0
}
package_auto () {
	return 0
}
EOF

	cat >$CFG.rc <<EOF
if [ "\$IM_CONFIG_PHASE"  =  1 ] ; then
	XMODIFIERS="@im=yong"
	if [ -e $GTK_IM_DETECT/*/immodules/im-yong.so ] ; then
		GTK_IM_MODULE="yong"
	else
		GTK_IM_MODULE="xim"
	fi
	if [ -e $QT5_PATH64/plugins/platforminputcontexts/libyongplatforminputcontextplugin.so ]; then
		QT_IM_MODULE="yong"
	else
		QT_IM_MODULE="xim"
	fi
fi
if [ "\$IM_CONFIG_PHASE"  =  2 ] ; then
	/usr/bin/yong -d
fi
EOF

}

function debian_uninstall2()
{
	rm -f $CFG.conf
	rm -f $CFG.rc
}

function debian_install()
{
	if [ -e /usr/share/im-config ] ; then
		debian_install2
		return
	fi
	
	CFG=/etc/X11/xinit/xinput.d/yong
	
	cat >$CFG <<EOF
XMODIFIERS="@im=yong"
XIM="yong"
XIM_PROGRAM="/usr/bin/yong"
XIM_ARGS=""
if [ -e $GTK_IM_DETECT/*/immodules/im-yong.so ] ; then
	GTK_IM_MODULE="yong"
else
	GTK_IM_MODULE="xim"
fi
QT_IM_MODULE="xim"
SHORT_DESC="yong"
PREFERENCE_PROGRAM="/usr/bin/yong-config"
EOF

	update-alternatives \
	       	--install /etc/X11/xinit/xinput.d/zh_CN xinput-zh_CN \
       		/etc/X11/xinit/xinput.d/yong 20
}

function debian_uninstall()
{
	if [ -e /usr/share/im-config ] ; then
		debian_uninstall2
		exit 0
	fi

	CFG=/etc/X11/xinit/xinput.d/yong

	update-alternatives --remove xinput-zh_CN /etc/X11/xinit/xinput.d/yong
	rm -rf $CFG
}

function debian_select()
{
	if [ -e /usr/bin/im-switch ] ; then
		im-switch -s yong
	elif [ -e /usr/bin/im-config ] ; then
		im-config -n yong
	else
		CFG=~/.xinputrc
		cat >$CFG <<EOF
XMODIFIERS="@im=yong"
XIM="yong"
XIM_PROGRAM="/usr/bin/yong"
XIM_ARGS=""
GTK_IM_MODULE="xim"
QT_IM_MODULE="xim"
SHORT_DESC="yong"
PREFERENCE_PROGRAM="/usr/bin/yong-config"
yong -d
EOF

	fi
}

function suse_install()
{
	CFG=/etc/X11/xim.d/yong

	cat >$CFG <<EOF
export XMODIFIERS="@im=yong"
if [ -e $GTK_IM_DETECT/*/immodules/im-yong.so ] ; then
	export GTK_IM_MODULE="yong"
else
	export GTK_IM_MODULE="xim"
fi
if [ -e $QT5_PATH64/plugins/platforminputcontexts/libyongplatforminputcontextplugin.so ]; then
	export QT_IM_MODULE="yong"
else
	export QT_IM_MODULE="xim"
fi
yong -d
EOF

}

function suse_uninstall()
{
	CFG=/etc/X11/xim.d/yong
	rm -rf $CFG
}

function suse_select()
{
	ln -sf $CFG ~/.xim
}

function legacy_install()
{
	return
}

function legacy_uninstall()
{
	return
}

function legacy_select()
{
	echo "set IM config yourself"
}

function ibus_install()
{
	IBUS_D=/usr/share/ibus/component
	if ! [ -d $IBUS_D ] ; then
		return
	fi

	sed "s%\/usr\/share\/yong%`pwd`%" >$IBUS_D/yong.xml <<EOF
<?xml version="1.0" encoding="utf-8"?>
<!-- filename: yong.xml -->
<component>
  <name>org.freedesktop.IBus.Yong</name>
  <description>Yong Component</description>
  <exec>/usr/bin/yong --ibus</exec>
  <version>2.6.0</version>
  <author>dgod</author>
  <homepage>http://yong.dgod.net</homepage>
  <textdomain>yong</textdomain>
  <engines>
    <engine>
      <name>yong</name>
      <language>zh_CN</language>
      <author>dgod</author>
      <icon>/usr/share/yong/skin/tray1.png</icon>
      <layout>us</layout>
      <longname>Yong</longname>
      <description>Yong Input Method</description>
      <setup>/usr/bin/yong --config</setup>
      <locale name="zh_CN">
        <longname>Yong</longname>
        <description>Yong输入法</description>
      </locale>
    </engine>
  </engines>
</component>
EOF

	sed "s%\/usr\/share\/yong%`pwd`%" >/usr/share/applications/ibus-setup-yong.desktop <<EOF
[Desktop Entry]
Name[zh_CN]=Yong输入法法配置工具
Name=Yong Setup
Comment[zh_CN]=设置Yong输入法首选项
Comment=Set Yong Preferences
Exec=/usr/bin/yong --config
Icon=/usr/share/yong/skin/tray1.png
NoDisplay=true
Type=Application
StartupNotify=true
EOF

}

function ibus_uninstall()
{
	if [ -f /usr/share/ibus/component/yong.xml ] ; then
		rm -f /usr/share/ibus/component/yong.xml
	fi
	if [ -f /usr/share/applications/ibus-setup-yong.desktop ] ; then
		rm -f /usr/share/applications/ibus-setup-yong.desktop
	fi
}

function gtk_install32()
{
	if ! [ -d l32 ] ; then
		return
	fi

	if ! [ -d l32/gtk-im ] ; then
		return
	fi
	
	if [ -z "$PATH_LIB32" ] ; then
		return
	fi
	
	cd l32
	
	if [ -f gtk-im/im-yong-gtk2.so -a -n "$GTK2_PATH32" ] ; then
		if [ -d $GTK2_PATH32/2.10.0/immodules ] ; then
			install gtk-im/im-yong-gtk2.so $GTK2_PATH32/2.10.0/immodules/im-yong.so
		fi
		
		if [ -x /usr/bin/update-gtk-immodules ] ;then
			/usr/bin/update-gtk-immodules $HOST_TRIPLET32
		elif [ -x /usr/bin/gtk-query-immodules-2.0-32 ] ;then
			/usr/bin/gtk-query-immodules-2.0-32 >$GTK2_IMMODULES32
		elif [ -x /usr/bin/gtk-query-immodules-2.0 ] ;then
			/usr/bin/gtk-query-immodules-2.0 >$GTK2_IMMODULES32
		elif [ -x /usr/lib/$HOST_TRIPLET32/libgtk2.0-0/gtk-query-immodules-2.0 ] ; then
			/usr/lib/$HOST_TRIPLET32/libgtk2.0-0/gtk-query-immodules-2.0 --update-cache
		else
			echo "update gtk2-im cache fail"
		fi
	fi
	
	if [ -f gtk-im/im-yong-gtk3.so  -a -n "$GTK3_PATH32" ] ; then
		if [ -d $GTK3_PATH32/3.0.0/immodules ] ; then
			install gtk-im/im-yong-gtk3.so $GTK3_PATH32/3.0.0/immodules/im-yong.so
		fi
		
		if [ -x /usr/bin/gtk-query-immodules-3.0-32 ] ;then
			/usr/bin/gtk-query-immodules-3.0-32 --update-cache
		elif [ -x /usr/bin/gtk-query-immodules-3.0 ] ;then
			/usr/bin/gtk-query-immodules-3.0 --update-cache
		elif [ -x /usr/lib/$HOST_TRIPLET32/libgtk-3-0/gtk-query-immodules-3.0 ] ; then
			/usr/lib/$HOST_TRIPLET32/libgtk-3-0/gtk-query-immodule-3.0 --update-cache
		else
			echo "update gtk3-im cache fail"
		fi
	fi
	
	if [ -f gtk-im/libimyong-gtk4.so -a -n "$GTK4_PATH32" ] ; then
		mkdir -p $GTK4_PATH32/4.0.0/immodules
		if [ -d $GTK4_PATH32/4.0.0/immodules ] ; then
			install gtk-im/libimyong-gtk4.so $GTK4_PATH32/4.0.0/immodules/libimyong.so
		fi
	fi

	cd - >/dev/null
}

function gtk_uninstall32()
{
	if ! [ -d l32 ] ; then
		return
	fi

	if ! [ -d l32/gtk-im ] ; then
		return
	fi
	
	if [ -z "$PATH_LIB32" ] ; then
		return
	fi
	
	if [ -f $GTK2_PATH32/2.10.0/immodules/im-yong.so ] ; then
		rm -rf $GTK2_PATH32/2.10.0/immodules/im-yong.so
		if [ -x /usr/bin/update-gtk-immodules ] ;then
			/usr/bin/update-gtk-immodules $HOST_TRIPLET32
		elif [ -x /usr/bin/gtk-query-immodules-2.0-32 ] ;then
			/usr/bin/gtk-query-immodules-2.0-32 >$GTK2_IMMODULES32
		elif [ -x /usr/bin/gtk-query-immodules-2.0 ] ;then
			/usr/bin/gtk-query-immodules-2.0 >$GTK2_IMMODULES32
		elif [ -x /usr/lib/$HOST_TRIPLET32/libgtk2.0-0/gtk-query-immodules-2.0 ] ; then
			/usr/lib/$HOST_TRIPLET32/libgtk2.0-0/gtk-query-immodules-2.0 --update-cache
		else
			echo "update gtk2-im cache fail"
		fi
	fi
	
	if [ -f $GTK3_PATH32/3.0.0/immodules/im-yong.so ] ; then
		rm -rf $GTK3_PATH32/3.0.0/immodules/im-yong.so
		
		if [ -x /usr/bin/gtk-query-immodules-3.0-32 ] ;then
			/usr/bin/gtk-query-immodules-3.0-32 --update-cache
		elif [ -x /usr/bin/gtk-query-immodules-3.0 ] ;then
			/usr/bin/gtk-query-immodules-3.0 --update-cache
		elif [ -x /usr/lib/$HOST_TRIPLET32/libgtk-3-0/gtk-query-immodules-3.0 ] ; then
			/usr/lib/$HOST_TRIPLET32/libgtk-3-0/gtk-query-immodules-3.0 --update-cache
		else
			echo "update gtk3-im cache fail"
		fi
	fi
	
	if [ -f $GTK4_PATH32/4.0.0/immodules/libimyong.so ] ; then
		rm -rf $GTK4_PATH32/4.0.0/immodules/libimyong.so
	fi
}

function gtk_install64()
{
	if ! [ -d l64 ] ; then
		return
	fi

	if ! [ -d l64/gtk-im ] ; then
		return
	fi
	
	if [ -z "$PATH_LIB64" ] ; then
		return
	fi
	
	cd l64
	
	if [ -f gtk-im/im-yong-gtk2.so  -a -n "$GTK2_PATH64" ] ; then
		if [ -d $GTK2_PATH64/2.10.0/immodules ] ; then
			install gtk-im/im-yong-gtk2.so $GTK2_PATH64/2.10.0/immodules/im-yong.so
		fi
	
		if [ -x /usr/bin/update-gtk-immodules ] ; then
			/usr/bin/update-gtk-immodules $HOST_TRIPLET64
		elif [ -x /usr/bin/gtk-query-immodules-2.0-64 ] ; then
			/usr/bin/gtk-query-immodules-2.0-64 >$GTK2_IMMODULES64
		elif [ -x /usr/bin/gtk-query-immodules-2.0 ] ; then
			/usr/bin/gtk-query-immodules-2.0 >$GTK2_IMMODULES64
		elif [ -x /usr/lib/$HOST_TRIPLET64/libgtk2.0-0/gtk-query-immodules-2.0 ] ; then
			/usr/lib/$HOST_TRIPLET64/libgtk2.0-0/gtk-query-immodules-2.0 --update-cache
		else
			echo "update gtk2-im cache fail"
		fi
	fi
	
	if [ -f gtk-im/im-yong-gtk3.so  -a -n "$GTK3_PATH64" ] ; then
		if [ -d $GTK3_PATH64/3.0.0 ] ; then
			install gtk-im/im-yong-gtk3.so $GTK3_PATH64/3.0.0/immodules/im-yong.so
		fi
		
		if [ -x /usr/bin/gtk-query-immodules-3.0-64 ] ; then
			/usr/bin/gtk-query-immodules-3.0-64 --update-cache
		elif [ -x /usr/bin/gtk-query-immodules-3.0 ] ; then
			/usr/bin/gtk-query-immodules-3.0 --update-cache
		elif [ -x /usr/lib/$HOST_TRIPLET64/libgtk-3-0/gtk-query-immodules-3.0 ] ; then
			/usr/lib/$HOST_TRIPLET64/libgtk-3-0/gtk-query-immodules-3.0 --update-cache
		else
			echo "update gtk3-im cache fail"
		fi
	fi
	
	if [ -f gtk-im/libimyong-gtk4.so -a -n "$GTK4_PATH64" ] ; then
		mkdir -p $GTK4_PATH64/4.0.0/immodules
		if [ -d $GTK4_PATH64/4.0.0/immodules ] ; then
			install gtk-im/libimyong-gtk4.so $GTK4_PATH64/4.0.0/immodules/libimyong.so
		fi
	fi
	
	cd - >/dev/null
}

function gtk_uninstall64()
{
	if [ -z "$PATH_LIB64" ] ; then
		return
	fi
	
	if [ -f $GTK2_PATH64/2.10.0/immodules/im-yong.so ] ; then
		rm -rf $GTK2_PATH64/2.10.0/immodules/im-yong.so
		if [ -x /usr/bin/update-gtk-immodules ] ;then
			/usr/bin/update-gtk-immodules $HOST_TRIPLET64
		elif [ -x /usr/bin/gtk-query-immodules-2.0-64 ] ;then
			/usr/bin/gtk-query-immodules-2.0-64 >$GTK2_IMMODULES64
		elif [ -x /usr/bin/gtk-query-immodules-2.0 ] ;then
			/usr/bin/gtk-query-immodules-2.0 >$GTK2_IMMODULES64
		elif [ -x /usr/lib/$HOST_TRIPLET64/libgtk2.0-0/gtk-query-immodules-2.0 ] ; then
			/usr/lib/$HOST_TRIPLET64/libgtk2.0-0/gtk-query-immodules-2.0 --update-cache
		else
			echo "update gtk2-im cache fail"
		fi
	fi
	
	if [ -f $GTK3_PATH64/3.0.0/immodules/im-yong.so ] ; then
		rm -rf $GTK3_PATH64/3.0.0/immodules/im-yong.so
		
		if [ -x /usr/bin/gtk-query-immodules-3.0-64 ] ;then
			/usr/bin/gtk-query-immodules-3.0-64 --update-cache
		elif [ -x /usr/bin/gtk-query-immodules-3.0 ] ;then
			/usr/bin/gtk-query-immodules-3.0 --update-cache
		elif [ -x /usr/lib/$HOST_TRIPLET64/libgtk-3-0/gtk-query-immodules-3.0 ] ; then
			/usr/lib/$HOST_TRIPLET64/libgtk-3-0/gtk-query-immodules-3.0 --update-cache
		else
			echo "update gtk3-im cache fail"
		fi
	fi
	
	if [ -f $GTK4_PATH64/4.0.0/immodules/libimyong.so ] ; then
		rm -rf $GTK4_PATH64/4.0.0/immodules/libimyong.so
	fi
}

function gtk_install()
{
	gtk_install32
	gtk_install64
}

function gtk_uninstall()
{
	gtk_uninstall32
	gtk_uninstall64
}

function locale_install()
{
	install locale/zh_CN.mo /usr/share/locale/zh_CN/LC_MESSAGES/yong.mo
}

function locale_uninstall()
{
	rm -rf /usr/share/locale/zh_CN/LC_MESSAGES/yong.mo
}

function detect_dist()
{
	if [ -n "$DIST" ] ; then
		echo $DIST forced
	else
		if [ -f /etc/fedora-release ] ; then
			DIST=fedora
		elif [ -f /etc/redhat-release ] ; then
			DIST=fedora
		elif [ -f /etc/centos-release ] ; then
			DIST=fedora
		elif [ -f /etc/redflag-release ] ; then
			DIST=fedora
		elif [ -f /etc/debian-release ] ; then
			DIST=debian
		elif [ -f /etc/SuSE-release ] ; then
			DIST=suse
		elif [ -f /etc/debian_version ] ; then
			DIST=debian
		elif [ `cat /etc/issue | grep Ubuntu | wc -l` != 0 ] ; then
			DIST=debian
		elif [ `cat /etc/issue | grep Mint | wc -l` != 0 ] ; then
			DIST=debian
		elif [ -f /etc/system-release ] ; then
			if [ `cat /etc/system-release | grep YLMF | wc -l` != 0 ] ; then
				DIST=debian
			else
				DIST=legacy
			fi
		else
			DIST=legacy
		fi
	fi
	echo DIST $DIST found
}

function detect_arch()
{
	HOST_MACHINE=`getconf LONG_BIT`
	ARCH=`uname -m`;
	
	if [ "$ARCH" = "i386" -o "$ARCH" = "i686" -o "$ARCH" = "x86_64" ] ; then
		HOST_ARCH=x86
		HOST_TRIPLET32=i386
		if [ $HOST_MACHINE -eq 64 ] ; then
			HOST_TRIPLET64=x86_64
		fi
	elif [ "$ARCH" = "arm" -o "$ARCH" = "armv8l" -o "$ARCH" = "aarch64" ] ; then
		HOST_ARCH=arm
		HOST_TRIPLET32=arm
		if [ $HOST_MACHINE -eq 64 ] ; then
			HOST_TRIPLET64=aarch64
		fi
	elif [ "$ARCH" = "loongarch" -o "$ARCH" = "loongarch64" ] ; then
		HOST_ARCH=loongarch
		HOST_TRIPLET32=loongarch
		if [ $HOST_MACHINE -eq 64 ] ; then
			HOST_TRIPLET64=loongarch64
		fi
	elif [ "$ARCH" = "mips" -o "$ARCH" = "mips64" ] ; then
		HOST_ARCH=mips
		HOST_TRIPLET32=mipsel
		if [ $HOST_MACHINE -eq 64 ] ; then
			HOST_TRIPLET64=mips64el
			HOST_TRIPLET32N=mipsn32el
		fi
	fi
	
	if [ "$DIST" = "fedora" ] ; then
		HOST_TRIPLET32+=-redhat-linux-gnu
		if [ $HOST_MACHINE -eq 64 ] ; then
			HOST_TRIPLET64+=-redhat-linux-gnu
			if ! [ -z "$HOST_TRIPLET32N" ]; then
				HOST_TRIPLET32N+=-redhat-linux-gnu
			fi
		fi
	fi
	
	if [ "$DIST" = "debian" ] ; then
		HOST_TRIPLET32+=-linux-gnu
		if [ $HOST_MACHINE -eq 64 ] ; then
			HOST_TRIPLET64+=-linux-gnu
			if ! [ -z "$HOST_TRIPLET32N" ]; then
				HOST_TRIPLET32N+=-linux-gnu
			fi
		fi
	fi
}

function detect_path()
{
	if [ -n "$PATH_LIB32" -o -n "$PATH_LIB64" ] ; then
		return
	fi
	if [ $HOST_MACHINE -eq 32 ] ; then
		PATH_LIB32=/usr/lib
	else
		if [ -d /usr/lib64 ]; then
			if [ -e /lib/ld-linux.so.2 ] ; then
				PATH_LIB32=/usr/lib
			fi
			if [ -e /lib64/ld-linux-x86-64.so.2 -o /lib64/ld-linux-loongarch-lp64d ] ; then
				PATH_LIB64=/usr/lib64
			fi
			if [ -d /usr/lib32 ]; then
				PATH_LIB32N=/usr/lib32
			fi
		elif [ -n "$HOST_TRIPLET64" -a -d /usr/lib/$HOST_TRIPLET64 ] ; then
			PATH_LIB64=/usr/lib/$HOST_TRIPLET64
			if [ -n "$HOST_TRIPLET32" -a -d /usr/lib/$HOST_TRIPLET32 ] ; then
				PATH_LIB32=/usr/lib/$HOST_TRIPLET32
			fi
		else
			PATH_LIB64=/usr/lib
		fi
	fi
}

function detect_ibus()
{
	if [ -d /usr/share/ibus/component ] ; then
		IBUS_STATUS=1
	else
		IBUS_STATUS=0
	fi
}

function detect_cfg()
{
	if [ "$DIST" = "fedora" ] ; then
		if [ -e /usr/bin/imsettings-info ] ; then
			CFG=/etc/X11/xinit/xinput.d/yong.conf
		elif [ -e /etc/X11/xinit/xinputrc ] ; then
			CFG=/etc/X11/xinit/xinput.d/yong.conf
		else
			CFG=/etc/X11/xinit/xinitrc.d/yong.sh
		fi
	elif [ "$DIST" = "debian" ] ; then
		if [ -e /usr/share/im-config ] ; then
			CFG=/usr/share/im-config/data/10_yong
		else
			CFG=/etc/X11/xinit/xinput.d/yong
		fi
	elif [ "$DIST" = "suse" ] ; then
		CFG=/etc/X11/xim.d/yong
	else
		CFG=
	fi
}

function detect_gtk2()
{
	if [ -z "$GTK2_PATH32" ] ; then
		if [ -n "$HOST_TRIPLET32" -a -d /usr/lib/$HOST_TRIPLET32/gtk-2.0 ] ; then
			GTK2_PATH32=/usr/lib/$HOST_TRIPLET32/gtk-2.0
		elif [ -d /usr/lib/gtk-2.0/ ] ; then
			if [ "$DIST" = "debian" -a $HOST_MACHINE -eq 64 ] ; then
				GTK2_PATH32=
			else
				GTK2_PATH32=/usr/lib/gtk-2.0
			fi
		fi
	fi
	
	if [ -z "$GTK2_PATH64" -a $HOST_MACHINE -eq 64 ] ; then
		if [ -n "$HOST_TRIPLET64" -a -d /usr/lib/$HOST_TRIPLET64/gtk-2.0 ] ; then
			GTK2_PATH64=/usr/lib/$HOST_TRIPLET64/gtk-2.0
		elif [ -d /usr/lib64/gtk-2.0/ ] ; then
			GTK2_PATH64=/usr/lib64/gtk-2.0
		else
			GTK2_PATH64=/usr/lib/gtk-2.0
		fi
		if [ "$GTK2_PATH32" = "$GTK2_PATH64" ] ; then
			GTK2_PATH32=
		fi
	fi
	
	if [ -z "$GTK_IM_DETECT" -a -n "$GTK2_PATH64" ] ; then
		GTK_IM_DETECT=$GTK2_PATH64
	fi
	if [ -z "$GTK_IM_DETECT" -a -n "$GTK2_PATH32" ] ; then
		GTK_IM_DETECT=$GTK2_PATH32
	fi
	
	if [ -z "$GTK2_IMMODULES32" -a -n "$GTK2_PATH32" ] ; then	
		if [ -e $GTK2_PATH32/2.10.0/immodules.cache ]; then
			GTK2_IMMODULES32=$GTK2_PATH32/2.10.0/immodules.cache
		elif [ -e $GTK2_PATH32/2.10.0/gtk.immodules ]; then
			GTK2_IMMODULES32=$GTK2_PATH32/2.10.0/gtk.immodules
		elif [ -e /etc/gtk-2.0/$HOST_TRIPLET32/gtk.immodules ]; then
			GTK2_IMMODULES32=/etc/gtk-2.0/$HOST_TRIPLET32/gtk.immodules
		elif [ -e /etc/gtk-2.0/gtk.immodules ]; then
			GTK2_IMMODULES32=/etc/gtk-2.0/gtk.immodules
		fi
	fi

	if [ -z "$GTK2_IMMODULES64" -a -n "$GTK2_PATH64" ] ; then
		if [ -e $GTK2_PATH64/2.10.0/immodules.cache ]; then
			GTK2_IMMODULES64=$GTK2_PATH64/2.10.0/immodules.cache
		elif [ -e $GTK2_PATH32/2.10.0/gtk.immodules ]; then
			GTK2_IMMODULES64=$GTK2_PATH64/2.10.0/gtk.immodules
		elif [ -e /etc/gtk-2.0/$HOST_TRIPLET32/gtk.immodules ]; then
			GTK2_IMMODULES64=/etc/gtk-2.0/$HOST_TRIPLET64/gtk.immodules
		elif [ -e /etc/gtk-2.0/gtk.immodules ]; then
			GTK2_IMMODULES64=/etc/gtk-2.0/gtk.immodules
		fi
	fi
}

function detect_gtk3()
{
	if [ -z "$GTK3_PATH32" ] ; then
		if [ -n "$HOST_TRIPLET32" -a -d /usr/lib/$HOST_TRIPLET32/gtk-3.0 ] ; then
			GTK3_PATH32=/usr/lib/$HOST_TRIPLET32/gtk-3.0
		elif [ -d /usr/lib/gtk-3.0/ ] ; then
			if [ "$DIST" = "debian" -a $HOST_MACHINE -eq 64 ] ; then
				GTK3_PATH32=
			else
				GTK3_PATH32=/usr/lib/gtk-3.0
			fi
		fi
	fi
	
	if [ -z "$GTK3_PATH64" -a $HOST_MACHINE -eq 64 ] ; then
		if [ -n "$HOST_TRIPLET64" -a -d /usr/lib/$HOST_TRIPLET64/gtk-3.0 ] ; then
			GTK3_PATH64=/usr/lib/$HOST_TRIPLET64/gtk-3.0
		elif [ -d /usr/lib64/gtk-3.0/ ] ; then
			GTK3_PATH64=/usr/lib64/gtk-3.0
		else
			GTK3_PATH64=/usr/lib/gtk-3.0
		fi
		if [ "$GTK3_PATH32" = "$GTK3_PATH64" ] ; then
			GTK3_PATH32=
		fi
	fi
	
	if [ -z "$GTK_IM_DETECT" -a -n "$GTK3_PATH64" ] ; then
		GTK_IM_DETECT=$GTK3_PATH64
	fi
	if [ -z "$GTK_IM_DETECT" -a -n "$GTK3_PATH32" ] ; then
		GTK_IM_DETECT=$GTK3_PATH32
	fi
}

function detect_gtk4(){
	if [ -z "$GTK4_PATH32" ] ; then
		if [ -n "$HOST_TRIPLET32" -a -d /usr/lib/$HOST_TRIPLET32/gtk-4.0 ] ; then
				GTK3_PATH32=/usr/lib/$HOST_TRIPLET32/gtk-4.0
		elif [ -d /usr/lib/gtk-4.0/ ] ; then
			if [ "$DIST" = "debian" -a $HOST_MACHINE -eq 64 ] ; then
				GTK4_PATH32=
			else
				GTK4_PATH32=/usr/lib/gtk-4.0
			fi
		fi
	fi
	
	if [ -z "$GTK4_PATH64" -a $HOST_MACHINE -eq 64 ] ; then
		if [ -n "$HOST_TRIPLET64" -a -d /usr/lib/$HOST_TRIPLET64/gtk-4.0 ] ; then
			GTK4_PATH64=/usr/lib/$HOST_TRIPLET64/gtk-4.0
		elif [ -d /usr/lib64/gtk-4.0/ ] ; then
			GTK4_PATH64=/usr/lib64/gtk-4.0
		else
			GTK4_PATH64=/usr/lib/gtk-4.0
		fi
		if [ "$GTK4_PATH32" = "$GTK4_PATH64" ] ; then
			GTK3_PATH32=
		fi
	fi
	
	if [ -z "$GTK_IM_DETECT" -a -n "$GTK4_PATH64" ] ; then
		GTK_IM_DETECT=$GTK4_PATH64
	fi
	if [ -z "$GTK_IM_DETECT" -a -n "$GTK4_PATH32" ] ; then
		GTK_IM_DETECT=$GTK4_PATH32
	fi
}

function detect_qt5()
{
	if [ -z "$QT5_PATH" ] ;then

		if [ -n "$HOST_TRIPLET32" -a -d /usr/lib/$HOST_TRIPLET32/qt5 ] ; then
			QT5_PATH32=/usr/lib/$HOST_TRIPLET32/qt5
		elif [ -d /usr/lib/qt5 ] ; then
			QT5_PATH32=/usr/lib/qt5
		fi

		if [ -n "$HOST_TRIPLET64" -a -d /usr/lib/$HOST_TRIPLET64/qt5 ] ; then
			QT5_PATH64=/usr/lib/$HOST_TRIPLET64/qt5
		elif [ -d /usr/lib64/qt5 ] ; then
			QT5_PATH64=/usr/lib64/qt5
		fi
	fi
}

function detect_qt6()
{
	if [ -z "$QT6_PATH" ] ;then

		if [ -n "$HOST_TRIPLET32" -a -d /usr/lib/$HOST_TRIPLET32/qt6 ] ; then
			QT6_PATH32=/usr/lib/$HOST_TRIPLET32/qt6
		elif [ -d /usr/lib/qt6 ] ; then
			QT5_PATH32=/usr/lib/qt6
		fi

		if [ -n "$HOST_TRIPLET64" -a -d /usr/lib/$HOST_TRIPLET64/qt6 ] ; then
			QT6_PATH64=/usr/lib/$HOST_TRIPLET64/qt6
		elif [ -d /usr/lib64/qt5 ] ; then
			QT6_PATH64=/usr/lib64/qt6
		fi
	fi
}

function detect_sysinfo()
{
	detect_dist
	detect_arch
	detect_path
	detect_ibus
	detect_cfg
	detect_gtk2
	detect_gtk3
	detect_gtk4
	detect_qt5
	detect_qt6
}

function display_sysinfo()
{
	echo "sysinfo:";
	echo "  DIST: $DIST"
	echo "  CFG: $CFG"
	echo "  HOST_ARCH: $HOST_ARCH"
	echo "  HOST_MACHINE: $HOST_MACHINE"
	echo "  PATH_LIB32: $PATH_LIB32"
	if ! [ -z "$PATH_LIB64" ]; then
		echo "  PATH_LIB64: $PATH_LIB64"
	fi
	if ! [ -z "$PATH_LIB32N" ]; then
		echo "  PATH_LIB32N: $PATH_LIB32N"
	fi
	echo "  HOST_TRIPLET32: $HOST_TRIPLET32"
	if ! [ -z "$HOST_TRIPLET64" ]; then
		echo "  HOST_TRIPLET64: $HOST_TRIPLET64"
	fi
	if ! [ -z "$HOST_TRIPLET32N" ]; then
		echo "  HOST_TRIPLET32N: $HOST_TRIPLET32N"
	fi
	echo "  IBUS_STATUS: $IBUS_STATUS"
	if ! [ -z "$GTK2_PATH32" ]; then
		echo "  GTK2_PATH32: $GTK2_PATH32"
	fi
	if ! [ -z "$GTK3_PATH32" ]; then
		echo "  GTK3_PATH32: $GTK3_PATH32"
	fi
	if ! [ -z "$GTK4_PATH32" ]; then
		echo "  GTK4_PATH32: $GTK4_PATH32"
	fi
	if ! [ -z "$GTK2_PATH64" ]; then
		echo "  GTK2_PATH64: $GTK2_PATH64"
	fi
	if ! [ -z "$GTK3_PATH64" ]; then
		echo "  GTK3_PATH64: $GTK3_PATH64"
	fi
	if ! [ -z "$GTK4_PATH64" ]; then
		echo "  GTK4_PATH64: $GTK4_PATH64"
	fi
	echo "  GTK2_IMMODULES32: $GTK2_IMMODULES32"
	if ! [ -z "$GTK2_IMMODULES64" ]; then
		echo "  GTK2_IMMODULES64: $GTK2_IMMODULES64"
	fi
	if ! [ -z "$QT5_PATH32" ]; then
		echo "  QT5_PATH32: $QT5_PATH32"
	fi
	if ! [ -z "$QT5_PATH64" ]; then
		echo "  QT5_PATH64: $QT5_PATH64"
	fi
	if ! [ -z "$QT6_PATH32" ]; then
		echo "  QT5_PATH32: $QT6_PATH32"
	fi
	if ! [ -z "$QT6_PATH64" ]; then
		echo "  QT6_PATH64: $QT6_PATH64"
	fi
}

function warn_sysinfo()
{
	if [ "$LANG" = "C" -o "$LANG" = "POSIX" ] ; then
		echo "yong not support C or POSIX locale"
		exit 0;
	fi
}

function install32()
{
	echo "install 32bit version"
	ln -sf `pwd`/l32/yong-gtk3 /usr/bin/yong
	ln -sf `pwd`/l32/yong-config-gtk3 /usr/bin/yong-config
	locale_install
	ibus_install
	gtk_install
	if [ $DIST = "fedora" ] ; then
		fedora_install
	elif [ $DIST = "debian" ] ; then
		debian_install
	elif [ $DIST = "suse" ] ; then
		suse_install
	elif [ $DIST = "legacy" ] ; then
		legacy_install
	fi
}

function install64()
{
	echo "install 64bit version"
	if ! [ $HOST_MACHINE -eq 64 ] ; then
		echo "not 64bit system"
		exit 1
	fi 
	if ! [ -d l64 ] ; then
		echo "no l64 found"
		exit 1
	fi
	
	ln -sf `pwd`/l64/yong-gtk3 /usr/bin/yong
	ln -sf `pwd`/l64/yong-config-gtk3 /usr/bin/yong-config
	
	locale_install
	ibus_install
	gtk_install
	if [ $DIST = "fedora" ] ; then
		fedora_install
	elif [ $DIST = "debian" ] ; then
		debian_install
	elif [ $DIST = "suse" ] ; then
		suse_install
	elif [ $DIST = "legacy" ] ; then
		legacy_install
	fi
}

function ensure_user_root()
{
	if ! [ $(id -un) = "root" -o $(id -ur) -eq 0 ] ; then
		echo "This command must run as root"
		exit 1
	fi
}

function warn_user_root()
{
	if [ $(id -un) = "root" -o $(id -ur) -eq 0 ] ; then
		echo "You are root now, maybe an error"
	fi
}

function wayland_select()
{
	if ! [ -f /usr/bin/ibus-daemon ] ; then
		mkdir -p ~/.config/autostart/
		cat >~/.config/autostart/yong.desktop <<EOF
[Desktop Entry]
Exec=/usr/bin/yong -d
Type=Application
Name=yong
EOF
	fi

	if [ -f /usr/bin/gnome-extensions -a -f gnome-shell/yong@dgod.net.shell-extension.zip ] ; then
		gnome-extensions install --force gnome-shell/yong@dgod.net.shell-extension.zip
		gnome-extensions enable yong@dgod.net
	fi
}

if [ $# != 1 ] ; then
	usage
	exit 0
fi

detect_sysinfo
warn_sysinfo

if [ $1 = "--install" ] ; then
	ensure_user_root
	if [ $HOST_MACHINE -eq 64 ] ; then
		install64
	else
		install32
	fi
elif [ $1 = "--install64" ] ; then
	ensure_user_root
	install64
elif [ $1 = "--install32" ] ; then
	ensure_user_root
	install32
elif [ $1 = "--uninstall" ] ; then
	ensure_user_root
	rm -rf /usr/bin/yong
	rm -rf /usr/bin/yong-config
	rm -rf /usr/bin/yong-vim
	ibus_uninstall
	gtk_uninstall
	locale_uninstall
	if [ $DIST = "fedora" ] ; then
		fedora_uninstall
	elif [ $DIST = "debian" ] ; then
		debian_uninstall
	elif [ $DIST = "suse" ] ; then
		suse_uninstall
	elif [ $DIST = "legacy" ] ; then
		legacy_uninstall
	fi
elif [ $1 = "--select" ] ; then
	warn_user_root
	if [ "$XDG_SESSION_TYPE" = "wayland" ] ; then
		wayland_select
		echo "--select wayland Done"
		exit
	fi
	if [ $DIST = "fedora" ] ; then
		fedora_select
	elif [ $DIST = "debian" ] ; then
		debian_select
	elif [ $DIST = "suse" ] ; then
		suse_select
	elif [ $DIST = "legacy" ] ; then
		legacy_select
	fi
elif [ $1 = "--sysinfo" ] ; then
	display_sysinfo
else
	usage
fi

echo "$1 Done"
