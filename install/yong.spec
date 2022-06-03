%define name		yong
%define version		2.6.0
%define release		0

%define prefix		/opt

%global _binaries_in_noarch_packages_terminate_build 0
%define _source_payload w9.gzdio
%define _binary_payload w9.gzdio

Name:		%{name}
Version:	%{version}
Release:	%{release}
Summary:	Yong Input Method
Summary(zh_CN):	Yong输入法
Packager:	dgod <dgod.osa@gmail.com>
URL:		http://yong.uueasy.com
Group:		User Interface/X
License:	DGOD
Source:		%{name}-%{version}.tar.gz
BuildRoot:	%{_builddir}/%{name}-%{version}-root
BuildArch:		noarch

%description
Yong Input Method, include yong(zrm), wubi, zhengma, erbi, english, gbk, pinyin, zhangma.

%description -l zh_CN
Yong输入法，包含永码(自然双拼)，五笔，郑码，二笔，英语，内码，拼音，张码。

%prep

%build

%install
mkdir -p ${RPM_BUILD_ROOT}/%{prefix}
cp -r ~/yong/install/yong ${RPM_BUILD_ROOT}/%{prefix}

%clean
[ ${RPM_BUILD_ROOT} != "/" ] && rm -rf ${RPM_BUILD_ROOT}

%files
%{prefix}/yong

%post
cd %{prefix}/yong
sh ./yong-tool.sh --install

%preun
if [ "$1" = "0" ] ; then
	cd %{prefix}/yong
	sh ./yong-tool.sh --uninstall
fi

%postun

%changelog

