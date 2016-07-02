const char *ui_menu_default=

"[root]\n"
"child=config tools im output help - exit\n"

"[config]\n"
"name=设置\n"
"exec=$CONFIG\n"

"[tools]\n"
"name=工具\n"
"child=stat sys update keymap mbo mbm mbe\n"

"[stat]\n"
"name=输入统计\n"
"exec=$STAT\n"

"[sys]\n"
"name=系统信息\n"
"exec=$SYSINFO\n"

"[update]\n"
"name=软件更新\n"
"exec=$GO(yong-config --update)\n"

"[keymap]\n"
"exec=$KEYMAP\n"

"[mbo]\n"
"name=码表优化\n"
"exec=$MBO\n"

"[mbm]\n"
"name=合并用户码表\n"
"exec=$MBM\n"

"[mbe]\n"
"name=编辑码表\n"
"exec=$MBEDIT\n"

"[im]\n"
"name=输入法\n"
"exec=$IMLIST\n"

"[output]\n"
"name=输出方式\n"
"exec=$OUTPUT\n"

"[help]\n"
"name=帮助\n"
"child=help0 help1 about\n"

"[help0]\n"
"exec=$HELP(main)\n"

"[help1]\n"
"exec=$HELP(?)\n"

"[about]\n"
"name=关于\n"
"exec=$ABOUT\n"

"[exit]\n"
"name=退出\n"
"exec=$EXIT\n"
;
