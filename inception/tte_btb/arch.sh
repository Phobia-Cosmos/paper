#!/bin/bash
# 这是一个 Bash 关联数组：key：hostname;value：人为定义的 微架构代号
declare -A nodes
nodes[kwik]="ZEN3"
nodes[ee-tik-cn102]="ZEN2"
nodes[ee-tik-cn108]="ZEN3"
nodes[undefined-MS-7E24]="ZEN5"
nodes[ee-tik-cn103]="COFFEE_RE"
nodes[ee-tik-cn104]="COFFEE"
nodes[ee-tik-cn109]="ROCKET"
nodes[ee-tik-cn114]="ALDER"
nodes[ee-tik-cn115]="ALDER"
nodes[ee-tik-cn116]="ICE"
nodes[ee-tik-cn120]="RAPTOR"
nodes[mini]="ZEN2"

ARCH=${nodes[$(hostname)]}

if [ -z $ARCH ]; then
  >&2 echo "[!!] Unknown mircoarchitecture for $(hostname)."
  >&2 echo "     Configure $0 first!"
  exit 1
fi

echo $ARCH
