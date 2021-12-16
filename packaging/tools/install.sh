#!/bin/bash
#
# This file is used to install database on linux systems. The operating system
# is required to use systemd to manage services at boot

set -e
#set -x

verMode=edge
pagMode=full

iplist=""
serverFqdn=""

# -----------------------Variables definition---------------------
script_dir=$(dirname $(readlink -f "$0"))
# Dynamic directory
data_dir="/var/lib/taos"
log_dir="/var/log/taos"

data_link_dir="/usr/local/taos/data"
log_link_dir="/usr/local/taos/log"

cfg_install_dir="/etc/taos"

bin_link_dir="/usr/bin"
lib_link_dir="/usr/lib"
lib64_link_dir="/usr/lib64"
inc_link_dir="/usr/include"

#install main path
install_main_dir="/usr/local/taos"

# old bin dir
bin_dir="/usr/local/taos/bin"

# v1.5 jar dir
#v15_java_app_dir="/usr/local/lib/taos"

service_config_dir="/etc/systemd/system"
nginx_port=6060
nginx_dir="/usr/local/nginxd"

# Color setting
RED='\033[0;31m'
GREEN='\033[1;32m'
GREEN_DARK='\033[0;32m'
GREEN_UNDERLINE='\033[4;32m'
NC='\033[0m'

csudo=""
if command -v sudo > /dev/null; then
    csudo="sudo"
fi

update_flag=0
prompt_force=0

initd_mod=0
service_mod=2
if pidof systemd &> /dev/null; then
    service_mod=0
elif $(which service &> /dev/null); then
    service_mod=1
    service_config_dir="/etc/init.d"
    if $(which chkconfig &> /dev/null); then
         initd_mod=1
    elif $(which insserv &> /dev/null); then
        initd_mod=2
    elif $(which update-rc.d &> /dev/null); then
        initd_mod=3
    else
        service_mod=2
    fi
else
    service_mod=2
fi


# get the operating system type for using the corresponding init file
# ubuntu/debian(deb), centos/fedora(rpm), others: opensuse, redhat, ..., no verification
#osinfo=$(awk -F= '/^NAME/{print $2}' /etc/os-release)
if [[ -e /etc/os-release ]]; then
  osinfo=$(cat /etc/os-release | grep "NAME" | cut -d '"' -f2)   ||:
else
  osinfo=""
fi
#echo "osinfo: ${osinfo}"
os_type=0
if echo $osinfo | grep -qwi "ubuntu" ; then
#  echo "This is ubuntu system"
  os_type=1
elif echo $osinfo | grep -qwi "debian" ; then
#  echo "This is debian system"
  os_type=1
elif echo $osinfo | grep -qwi "Kylin" ; then
#  echo "This is Kylin system"
  os_type=1
elif  echo $osinfo | grep -qwi "centos" ; then
#  echo "This is centos system"
  os_type=2
elif echo $osinfo | grep -qwi "fedora" ; then
#  echo "This is fedora system"
  os_type=2
elif echo $osinfo | grep -qwi "Linx" ; then
#  echo "This is Linx system"
  os_type=1
  service_mod=0
  initd_mod=0
  service_config_dir="/etc/systemd/system"
else
  echo " osinfo: ${osinfo}"
  echo " This is an officially unverified linux system,"
  echo " if there are any problems with the installation and operation, "
  echo " please feel free to contact taosdata.com for support."
  os_type=1
fi


# =============================  get input parameters =================================================

# install.sh -v [server | client]  -e [yes | no] -i [systemd | service | ...]

# set parameters by default value
interactiveFqdn=yes   # [yes | no]
verType=server        # [server | client]
initType=systemd      # [systemd | service | ...]

while getopts "hv:e:i:" arg
do
  case $arg in
    e)
      #echo "interactiveFqdn=$OPTARG"
      interactiveFqdn=$( echo $OPTARG )
      ;;
    v)
      #echo "verType=$OPTARG"
      verType=$(echo $OPTARG)
      ;;
    i)
      #echo "initType=$OPTARG"
      initType=$(echo $OPTARG)
      ;;
    h)
      echo "Usage: `basename $0` -v [server | client]  -e [yes | no]"
      exit 0
      ;;
    ?) #unknow option
      echo "unkonw argument"
      exit 1
      ;;
  esac
done

#echo "verType=${verType} interactiveFqdn=${interactiveFqdn}"

function kill_process() {
  pid=$(ps -ef | grep "$1" | grep -v "grep" | awk '{print $2}')
  if [ -n "$pid" ]; then
    ${csudo} kill -9 $pid   || :
  fi
}

function install_main_path() {
    #create install main dir and all sub dir
    ${csudo} rm -rf ${install_main_dir}    || :
    ${csudo} mkdir -p ${install_main_dir}
    ${csudo} mkdir -p ${install_main_dir}/cfg
    ${csudo} mkdir -p ${install_main_dir}/bin
    ${csudo} mkdir -p ${install_main_dir}/connector
    ${csudo} mkdir -p ${install_main_dir}/driver
    ${csudo} mkdir -p ${install_main_dir}/examples
    ${csudo} mkdir -p ${install_main_dir}/include
    ${csudo} mkdir -p ${install_main_dir}/init.d
    if [ "$verMode" == "cluster" ]; then
        ${csudo} mkdir -p ${nginx_dir}
    fi

    if [[ -e ${script_dir}/email ]]; then
      ${csudo} cp ${script_dir}/email ${install_main_dir}/ ||:
    fi
}

function install_bin() {
    # Remove links
    ${csudo} rm -f ${bin_link_dir}/taos     || :
    ${csudo} rm -f ${bin_link_dir}/taosd    || :
    ${csudo} rm -f ${bin_link_dir}/taosadapter  || :
    ${csudo} rm -f ${bin_link_dir}/taosdemo || :
    ${csudo} rm -f ${bin_link_dir}/taosdump || :
    ${csudo} rm -f ${bin_link_dir}/rmtaos   || :
    ${csudo} rm -f ${bin_link_dir}/tarbitrator  || :
    ${csudo} rm -f ${bin_link_dir}/set_core     || :
    ${csudo} rm -f ${bin_link_dir}/run_taosd.sh || :

    ${csudo} cp -r ${script_dir}/bin/* ${install_main_dir}/bin && ${csudo} chmod 0555 ${install_main_dir}/bin/*

    #Make link
    [ -x ${install_main_dir}/bin/taos ] && ${csudo} ln -s ${install_main_dir}/bin/taos ${bin_link_dir}/taos                      || :
    [ -x ${install_main_dir}/bin/taosd ] && ${csudo} ln -s ${install_main_dir}/bin/taosd ${bin_link_dir}/taosd                   || :
    [ -x ${install_main_dir}/bin/taosadapter ] && ${csudo} ln -s ${install_main_dir}/bin/taosadapter ${bin_link_dir}/taosadapter || :
    [ -x ${install_main_dir}/bin/taosdemo ] && ${csudo} ln -s ${install_main_dir}/bin/taosdemo ${bin_link_dir}/taosdemo          || :
    [ -x ${install_main_dir}/bin/taosdump ] && ${csudo} ln -s ${install_main_dir}/bin/taosdump ${bin_link_dir}/taosdump          || :
    [ -x ${install_main_dir}/bin/remove.sh ] && ${csudo} ln -s ${install_main_dir}/bin/remove.sh ${bin_link_dir}/rmtaos          || :
    [ -x ${install_main_dir}/bin/set_core.sh ] && ${csudo} ln -s ${install_main_dir}/bin/set_core.sh ${bin_link_dir}/set_core    || :
    [ -x ${install_main_dir}/bin/run_taosd.sh ] && ${csudo} ln -s ${install_main_dir}/bin/run_taosd.sh ${bin_link_dir}/run_taosd.sh     || :
    [ -x ${install_main_dir}/bin/tarbitrator ] && ${csudo} ln -s ${install_main_dir}/bin/tarbitrator ${bin_link_dir}/tarbitrator || :

    if [ "$verMode" == "cluster" ]; then
        ${csudo} cp -r ${script_dir}/nginxd/* ${nginx_dir} && ${csudo} chmod 0555 ${nginx_dir}/*
        ${csudo} mkdir -p ${nginx_dir}/logs
        ${csudo} chmod 777 ${nginx_dir}/sbin/nginx
    fi
}

function install_lib() {
    # Remove links
    ${csudo} rm -f ${lib_link_dir}/libtaos.*         || :
    ${csudo} rm -f ${lib64_link_dir}/libtaos.*       || :
    #${csudo} rm -rf ${v15_java_app_dir}              || :
    ${csudo} cp -rf ${script_dir}/driver/* ${install_main_dir}/driver && ${csudo} chmod 777 ${install_main_dir}/driver/*

    ${csudo} ln -s ${install_main_dir}/driver/libtaos.* ${lib_link_dir}/libtaos.so.1
    ${csudo} ln -s ${lib_link_dir}/libtaos.so.1 ${lib_link_dir}/libtaos.so

    if [[ -d ${lib64_link_dir} && ! -e ${lib64_link_dir}/libtaos.so ]]; then
      ${csudo} ln -s ${install_main_dir}/driver/libtaos.* ${lib64_link_dir}/libtaos.so.1       || :
      ${csudo} ln -s ${lib64_link_dir}/libtaos.so.1 ${lib64_link_dir}/libtaos.so               || :
    fi

    #if [ "$verMode" == "cluster" ]; then
    #    # Compatible with version 1.5
    #    ${csudo} mkdir -p ${v15_java_app_dir}
    #    ${csudo} ln -s ${install_main_dir}/connector/taos-jdbcdriver-1.0.2-dist.jar ${v15_java_app_dir}/JDBCDriver-1.0.2-dist.jar
    #    ${csudo} chmod 777 ${v15_java_app_dir} || :
    #fi

    ${csudo} ldconfig
}

function install_avro() {
    if [ "$osType" != "Darwin" ]; then
        avro_dir=${script_dir}/avro
        if [ -f "${avro_dir}/lib/libavro.so.23.0.0" ] && [ -d /usr/local/$1 ]; then
            ${csudo} /usr/bin/install -c -d /usr/local/$1
            ${csudo} /usr/bin/install -c -m 755 ${avro_dir}/lib/libavro.so.23.0.0 /usr/local/$1
            ${csudo} ln -sf /usr/local/$1/libavro.so.23.0.0 /usr/local/$1/libavro.so.23
            ${csudo} ln -sf /usr/local/$1/libavro.so.23 /usr/local/$1/libavro.so

            ${csudo} /usr/bin/install -c -d /usr/local/$1
            [ -f ${avro_dir}/lib/libavro.a ] &&
                ${csudo} /usr/bin/install -c -m 755 ${avro_dir}/lib/libavro.a /usr/local/$1

            if [ -d /etc/ld.so.conf.d ]; then
                echo "/usr/local/$1" | ${csudo} tee /etc/ld.so.conf.d/libavro.conf > /dev/null || echo -e "failed to write /etc/ld.so.conf.d/libavro.conf"
                ${csudo} ldconfig
            else
                echo "/etc/ld.so.conf.d not found!"
            fi
        fi
    fi
}

function install_jemalloc() {
    jemalloc_dir=${script_dir}/jemalloc

    if [ -d ${jemalloc_dir} ]; then
        ${csudo} /usr/bin/install -c -d /usr/local/bin

        if [ -f ${jemalloc_dir}/bin/jemalloc-config ]; then
            ${csudo} /usr/bin/install -c -m 755 ${jemalloc_dir}/bin/jemalloc-config /usr/local/bin
        fi
        if [ -f ${jemalloc_dir}/bin/jemalloc.sh ]; then
            ${csudo} /usr/bin/install -c -m 755 ${jemalloc_dir}/bin/jemalloc.sh /usr/local/bin
        fi
        if [ -f ${jemalloc_dir}/bin/jeprof ]; then
            ${csudo} /usr/bin/install -c -m 755 ${jemalloc_dir}/bin/jeprof /usr/local/bin
        fi
        if [ -f ${jemalloc_dir}/include/jemalloc/jemalloc.h ]; then
            ${csudo} /usr/bin/install -c -d /usr/local/include/jemalloc
            ${csudo} /usr/bin/install -c -m 644 ${jemalloc_dir}/include/jemalloc/jemalloc.h /usr/local/include/jemalloc
        fi
        if [ -f ${jemalloc_dir}/lib/libjemalloc.so.2 ]; then
            ${csudo} /usr/bin/install -c -d /usr/local/lib
            ${csudo} /usr/bin/install -c -m 755 ${jemalloc_dir}/lib/libjemalloc.so.2 /usr/local/lib
            ${csudo} ln -sf libjemalloc.so.2 /usr/local/lib/libjemalloc.so
            ${csudo} /usr/bin/install -c -d /usr/local/lib
            if [ -f ${jemalloc_dir}/lib/libjemalloc.a ]; then
                ${csudo} /usr/bin/install -c -m 755 ${jemalloc_dir}/lib/libjemalloc.a /usr/local/lib
            fi
            if [ -f ${jemalloc_dir}/lib/libjemalloc_pic.a ]; then
                ${csudo} /usr/bin/install -c -m 755 ${jemalloc_dir}/lib/libjemalloc_pic.a /usr/local/lib
            fi
            if [ -f ${jemalloc_dir}/lib/libjemalloc_pic.a ]; then
                ${csudo} /usr/bin/install -c -d /usr/local/lib/pkgconfig
                ${csudo} /usr/bin/install -c -m 644 ${jemalloc_dir}/lib/pkgconfig/jemalloc.pc /usr/local/lib/pkgconfig
            fi
        fi
        if [ -f ${jemalloc_dir}/share/doc/jemalloc/jemalloc.html ]; then
            ${csudo} /usr/bin/install -c -d /usr/local/share/doc/jemalloc
            ${csudo} /usr/bin/install -c -m 644 ${jemalloc_dir}/share/doc/jemalloc/jemalloc.html /usr/local/share/doc/jemalloc
        fi
        if [ -f ${jemalloc_dir}/share/man/man3/jemalloc.3 ]; then
            ${csudo} /usr/bin/install -c -d /usr/local/share/man/man3
            ${csudo} /usr/bin/install -c -m 644 ${jemalloc_dir}/share/man/man3/jemalloc.3 /usr/local/share/man/man3
        fi

        if [ -d /etc/ld.so.conf.d ]; then
            echo "/usr/local/lib" | ${csudo} tee /etc/ld.so.conf.d/jemalloc.conf > /dev/null || echo -e "failed to write /etc/ld.so.conf.d/jemalloc.conf"
            ${csudo} ldconfig
        else
            echo "/etc/ld.so.conf.d not found!"
        fi
    fi
}

function install_header() {
    ${csudo} rm -f ${inc_link_dir}/taos.h ${inc_link_dir}/taosdef.h ${inc_link_dir}/taoserror.h    || :
    ${csudo} cp -f ${script_dir}/inc/* ${install_main_dir}/include && ${csudo} chmod 644 ${install_main_dir}/include/*
    ${csudo} ln -s ${install_main_dir}/include/taos.h ${inc_link_dir}/taos.h
    ${csudo} ln -s ${install_main_dir}/include/taosdef.h ${inc_link_dir}/taosdef.h
    ${csudo} ln -s ${install_main_dir}/include/taoserror.h ${inc_link_dir}/taoserror.h
}

function add_newHostname_to_hosts() {
  localIp="127.0.0.1"
  OLD_IFS="$IFS"
  IFS=" "
  iphost=$(cat /etc/hosts | grep $1 | awk '{print $1}')
  arr=($iphost)
  IFS="$OLD_IFS"
  for s in "${arr[@]}"
  do
    if [[ "$s" == "$localIp" ]]; then
      return
    fi
  done
  ${csudo} echo "127.0.0.1  $1" >> /etc/hosts   ||:
}

function set_hostname() {
  echo -e -n "${GREEN}Please enter one hostname(must not be 'localhost')${NC}:"
    read newHostname
  while true; do
    if [[ ! -z "$newHostname" && "$newHostname" != "localhost" ]]; then
      break
    else
      read -p "Please enter one hostname(must not be 'localhost'):" newHostname
    fi
  done

  ${csudo} hostname $newHostname ||:
  retval=`echo $?`
  if [[ $retval != 0 ]]; then
   echo
   echo "set hostname fail!"
   return
  fi
  #echo -e -n "$(hostnamectl status --static)"
  #echo -e -n "$(hostnamectl status --transient)"
  #echo -e -n "$(hostnamectl status --pretty)"

  #ubuntu/centos /etc/hostname
  if [[ -e /etc/hostname ]]; then
    ${csudo} echo $newHostname > /etc/hostname   ||:
  fi

  #debian: #HOSTNAME=yourname
  if [[ -e /etc/sysconfig/network ]]; then
    ${csudo} sed -i -r "s/#*\s*(HOSTNAME=\s*).*/\1$newHostname/" /etc/sysconfig/network   ||:
  fi

  ${csudo} sed -i -r "s/#*\s*(fqdn\s*).*/\1$newHostname/" ${cfg_install_dir}/taos.cfg
  serverFqdn=$newHostname

  if [[ -e /etc/hosts ]]; then
    add_newHostname_to_hosts $newHostname
  fi
}

function is_correct_ipaddr() {
  newIp=$1
  OLD_IFS="$IFS"
  IFS=" "
  arr=($iplist)
  IFS="$OLD_IFS"
  for s in "${arr[@]}"
  do
   if [[ "$s" == "$newIp" ]]; then
     return 0
   fi
  done

  return 1
}

function set_ipAsFqdn() {
  iplist=$(ip address |grep inet |grep -v inet6 |grep -v 127.0.0.1 |awk '{print $2}' |awk -F "/" '{print $1}') ||:
  if [ -z "$iplist" ]; then
    iplist=$(ifconfig |grep inet |grep -v inet6 |grep -v 127.0.0.1 |awk '{print $2}' |awk -F ":" '{print $2}') ||:
  fi

  if [ -z "$iplist" ]; then
    echo
    echo -e -n "${GREEN}Unable to get local ip, use 127.0.0.1${NC}"
    localFqdn="127.0.0.1"
    # Write the local FQDN to configuration file
    ${csudo} sed -i -r "s/#*\s*(fqdn\s*).*/\1$localFqdn/" ${cfg_install_dir}/taos.cfg
    serverFqdn=$localFqdn
    echo
    return
  fi

  echo -e -n "${GREEN}Please choose an IP from local IP list${NC}:"
  echo
  echo -e -n "${GREEN}$iplist${NC}"
  echo
  echo
  echo -e -n "${GREEN}Notes: if IP is used as the node name, data can NOT be migrated to other machine directly${NC}:"
  read localFqdn
    while true; do
      if [ ! -z "$localFqdn" ]; then
        # Check if correct ip address
        is_correct_ipaddr $localFqdn
        retval=`echo $?`
        if [[ $retval != 0 ]]; then
          read -p "Please choose an IP from local IP list:" localFqdn
        else
          # Write the local FQDN to configuration file
          ${csudo} sed -i -r "s/#*\s*(fqdn\s*).*/\1$localFqdn/" ${cfg_install_dir}/taos.cfg
          serverFqdn=$localFqdn
          break
        fi
      else
        read -p "Please choose an IP from local IP list:" localFqdn
      fi
    done
}

function local_fqdn_check() {
  #serverFqdn=$(hostname)
  echo
  echo -e -n "System hostname is: ${GREEN}$serverFqdn${NC}"
  echo
  if [[ "$serverFqdn" == "" ]] || [[ "$serverFqdn" == "localhost"  ]]; then
    echo -e -n "${GREEN}It is strongly recommended to configure a hostname for this machine ${NC}"
    echo

    while true
    do
        read -r -p "Set hostname now? [Y/n] " input
        if [ ! -n "$input" ]; then
            set_hostname
            break
        else
            case $input in
                [yY][eE][sS]|[yY])
                    set_hostname
                    break
                    ;;

                [nN][oO]|[nN])
                    set_ipAsFqdn
                    break
                    ;;

                *)
                    echo "Invalid input..."
                    ;;
            esac
        fi
    done
  fi
}

function install_taosadapter_config() {
    if [ ! -f "${cfg_install_dir}/taosadapter.toml" ]; then
        ${csudo} mkdir -p ${cfg_install_dir}
        [ -f ${script_dir}/cfg/taosadapter.toml ] && ${csudo} cp ${script_dir}/cfg/taosadapter.toml ${cfg_install_dir}
        [ -f ${cfg_install_dir}/taosadapter.toml ] && ${csudo} chmod 644 ${cfg_install_dir}/taosadapter.toml
    fi

    [ -f ${script_dir}/cfg/taosadapter.toml ] &&
        ${csudo} cp -f ${script_dir}/cfg/taosadapter.toml ${cfg_install_dir}/taosadapter.toml.new

    [ -f ${cfg_install_dir}/taosadapter.toml ] &&
        ${csudo} ln -s ${cfg_install_dir}/taosadapter.toml ${install_main_dir}/cfg/taosadapter.toml

    [ ! -z $1 ] && return 0 || : # only install client

}

function install_config() {
    #${csudo} rm -f ${install_main_dir}/cfg/taos.cfg     || :

    if [ ! -f "${cfg_install_dir}/taos.cfg" ]; then
        ${csudo} mkdir -p ${cfg_install_dir}
        [ -f ${script_dir}/cfg/taos.cfg ] && ${csudo} cp ${script_dir}/cfg/taos.cfg ${cfg_install_dir}
        ${csudo} chmod 644 ${cfg_install_dir}/*
    fi

    ${csudo} cp -f ${script_dir}/cfg/taos.cfg ${cfg_install_dir}/taos.cfg.new
    ${csudo} ln -s ${cfg_install_dir}/taos.cfg ${install_main_dir}/cfg

    [ ! -z $1 ] && return 0 || : # only install client

    if ((${update_flag}==1)); then
        return 0
    fi

    if [ "$interactiveFqdn" == "no" ]; then
        return 0
    fi

    local_fqdn_check

    #FQDN_FORMAT="(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)"
    #FQDN_FORMAT="(:[1-6][0-9][0-9][0-9][0-9]$)"
    #PORT_FORMAT="(/[1-6][0-9][0-9][0-9][0-9]?/)"
    #FQDN_PATTERN=":[0-9]{1,5}$"

    # first full-qualified domain name (FQDN) for TDengine cluster system
    echo
    echo -e -n "${GREEN}Enter FQDN:port (like h1.taosdata.com:6030) of an existing TDengine cluster node to join${NC}"
    echo
    echo -e -n "${GREEN}OR leave it blank to build one${NC}:"
    read firstEp
    while true; do
        if [ ! -z "$firstEp" ]; then
            # check the format of the firstEp
            #if [[ $firstEp == $FQDN_PATTERN ]]; then
                # Write the first FQDN to configuration file
                ${csudo} sed -i -r "s/#*\s*(firstEp\s*).*/\1$firstEp/" ${cfg_install_dir}/taos.cfg
                break
            #else
            #    read -p "Please enter the correct FQDN:port: " firstEp
            #fi
        else
            break
        fi
    done

    # user email
    #EMAIL_PATTERN='^[A-Za-z0-9\u4e00-\u9fa5]+@[a-zA-Z0-9_-]+(\.[a-zA-Z0-9_-]+)+$'
    #EMAIL_PATTERN='^[\w-]+(\.[\w-]+)*@[\w-]+(\.[\w-]+)+$'
    #EMAIL_PATTERN="^[\w-]+(\.[\w-]+)*@[\w-]+(\.[\w-]+)+$"
    echo
    echo -e -n "${GREEN}Enter your email address for priority support or enter empty to skip${NC}: "
    read emailAddr
    while true; do
        if [ ! -z "$emailAddr" ]; then
            # check the format of the emailAddr
            #if [[ "$emailAddr" =~ $EMAIL_PATTERN ]]; then
                # Write the email address to temp file
                email_file="${install_main_dir}/email"
                ${csudo} bash -c "echo $emailAddr > ${email_file}"
                break
            #else
            #    read -p "Please enter the correct email address: " emailAddr
            #fi
        else
            break
        fi
    done
}


function install_log() {
    ${csudo} rm -rf ${log_dir}  || :
    ${csudo} mkdir -p ${log_dir} && ${csudo} chmod 777 ${log_dir}

    ${csudo} ln -s ${log_dir} ${install_main_dir}/log
}

function install_data() {
    ${csudo} mkdir -p ${data_dir}

    ${csudo} ln -s ${data_dir} ${install_main_dir}/data
}

function install_connector() {
    ${csudo} cp -rf ${script_dir}/connector/ ${install_main_dir}/
}

function install_examples() {
    if [ -d ${script_dir}/examples ]; then
        ${csudo} cp -rf ${script_dir}/examples/* ${install_main_dir}/examples
    fi
}

function clean_service_on_sysvinit() {
    #restart_config_str="taos:2345:respawn:${service_config_dir}/taosd start"
    #${csudo} sed -i "\|${restart_config_str}|d" /etc/inittab || :

    if pidof taosd &> /dev/null; then
        ${csudo} service taosd stop || :
    fi

    if pidof tarbitrator &> /dev/null; then
        ${csudo} service tarbitratord stop || :
    fi

    if ((${initd_mod}==1)); then
      if [ -e ${service_config_dir}/taosd ]; then
        ${csudo} chkconfig --del taosd || :
      fi

      if [ -e ${service_config_dir}/tarbitratord ]; then
        ${csudo} chkconfig --del tarbitratord || :
      fi
    elif ((${initd_mod}==2)); then
      if [ -e ${service_config_dir}/taosd ]; then
        ${csudo} insserv -r taosd || :
      fi
      if [ -e ${service_config_dir}/tarbitratord ]; then
        ${csudo} insserv -r tarbitratord || :
      fi
    elif ((${initd_mod}==3)); then
      if [ -e ${service_config_dir}/taosd ]; then
        ${csudo} update-rc.d -f taosd remove || :
      fi
      if [ -e ${service_config_dir}/tarbitratord ]; then
        ${csudo} update-rc.d -f tarbitratord remove || :
      fi
    fi

    ${csudo} rm -f ${service_config_dir}/taosd || :
    ${csudo} rm -f ${service_config_dir}/tarbitratord || :

    if $(which init &> /dev/null); then
        ${csudo} init q || :
    fi
}

function install_service_on_sysvinit() {
    clean_service_on_sysvinit
    sleep 1

    # Install taosd service

    if ((${os_type}==1)); then
        ${csudo} cp -f ${script_dir}/init.d/taosd.deb ${install_main_dir}/init.d/taosd
        ${csudo} cp    ${script_dir}/init.d/taosd.deb ${service_config_dir}/taosd && ${csudo} chmod a+x ${service_config_dir}/taosd
        ${csudo} cp -f ${script_dir}/init.d/tarbitratord.deb ${install_main_dir}/init.d/tarbitratord
        ${csudo} cp    ${script_dir}/init.d/tarbitratord.deb ${service_config_dir}/tarbitratord && ${csudo} chmod a+x ${service_config_dir}/tarbitratord
    elif ((${os_type}==2)); then
        ${csudo} cp -f ${script_dir}/init.d/taosd.rpm ${install_main_dir}/init.d/taosd
        ${csudo} cp    ${script_dir}/init.d/taosd.rpm ${service_config_dir}/taosd && ${csudo} chmod a+x ${service_config_dir}/taosd
        ${csudo} cp -f ${script_dir}/init.d/tarbitratord.rpm ${install_main_dir}/init.d/tarbitratord
        ${csudo} cp    ${script_dir}/init.d/tarbitratord.rpm ${service_config_dir}/tarbitratord && ${csudo} chmod a+x ${service_config_dir}/tarbitratord
    fi

    #restart_config_str="taos:2345:respawn:${service_config_dir}/taosd start"
    #${csudo} grep -q -F "$restart_config_str" /etc/inittab || ${csudo} bash -c "echo '${restart_config_str}' >> /etc/inittab"

    if ((${initd_mod}==1)); then
        ${csudo} chkconfig --add taosd || :
        ${csudo} chkconfig --level 2345 taosd on || :
        ${csudo} chkconfig --add tarbitratord || :
        ${csudo} chkconfig --level 2345 tarbitratord on || :
    elif ((${initd_mod}==2)); then
        ${csudo} insserv taosd || :
        ${csudo} insserv -d taosd || :
        ${csudo} insserv tarbitratord || :
        ${csudo} insserv -d tarbitratord || :
    elif ((${initd_mod}==3)); then
        ${csudo} update-rc.d taosd defaults || :
        ${csudo} update-rc.d tarbitratord defaults || :
    fi
}

function clean_service_on_systemd() {
    taosd_service_config="${service_config_dir}/taosd.service"
    if systemctl is-active --quiet taosd; then
        echo "TDengine is running, stopping it..."
        ${csudo} systemctl stop taosd &> /dev/null || echo &> /dev/null
    fi
    ${csudo} systemctl disable taosd &> /dev/null || echo &> /dev/null
    ${csudo} rm -f ${taosd_service_config}

    tarbitratord_service_config="${service_config_dir}/tarbitratord.service"
    if systemctl is-active --quiet tarbitratord; then
        echo "tarbitrator is running, stopping it..."
        ${csudo} systemctl stop tarbitratord &> /dev/null || echo &> /dev/null
    fi
    ${csudo} systemctl disable tarbitratord &> /dev/null || echo &> /dev/null
    ${csudo} rm -f ${tarbitratord_service_config}

    if [ "$verMode" == "cluster" ]; then
        nginx_service_config="${service_config_dir}/nginxd.service"
        if systemctl is-active --quiet nginxd; then
            echo "Nginx for TDengine is running, stopping it..."
            ${csudo} systemctl stop nginxd &> /dev/null || echo &> /dev/null
        fi
        ${csudo} systemctl disable nginxd &> /dev/null || echo &> /dev/null
        ${csudo} rm -f ${nginx_service_config}
    fi
}

# taos:2345:respawn:/etc/init.d/taosd start

function install_service_on_systemd() {
    clean_service_on_systemd

    [ -f ${script_dir}/cfg/taosd.service ] &&\
        ${csudo} cp ${script_dir}/cfg/taosd.service \
        ${service_config_dir}/ || :
    ${csudo} systemctl daemon-reload
    
    #taosd_service_config="${service_config_dir}/taosd.service"
    #${csudo} bash -c "echo '[Unit]'                             >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'Description=TDengine server service' >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'After=network-online.target taosadapter.service'        >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'Wants=network-online.target taosadapter.service'        >> ${taosd_service_config}"
    #${csudo} bash -c "echo                                      >> ${taosd_service_config}"
    #${csudo} bash -c "echo '[Service]'                          >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'Type=simple'                        >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'ExecStart=/usr/bin/taosd'           >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'ExecStartPre=/usr/local/taos/bin/startPre.sh'         >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'TimeoutStopSec=1000000s'            >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'LimitNOFILE=infinity'               >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'LimitNPROC=infinity'                >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'LimitCORE=infinity'                 >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'TimeoutStartSec=0'                  >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'StandardOutput=null'                >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'Restart=always'                     >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'StartLimitBurst=3'                  >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'StartLimitInterval=60s'             >> ${taosd_service_config}"
    ##${csudo} bash -c "echo 'StartLimitIntervalSec=60s'          >> ${taosd_service_config}"
    #${csudo} bash -c "echo                                      >> ${taosd_service_config}"
    #${csudo} bash -c "echo '[Install]'                          >> ${taosd_service_config}"
    #${csudo} bash -c "echo 'WantedBy=multi-user.target'         >> ${taosd_service_config}"
    ${csudo} systemctl enable taosd

    [ -f ${script_dir}/cfg/tarbitratord.service ] &&\
        ${csudo} cp ${script_dir}/cfg/tarbitratord.service \
        ${service_config_dir}/ || :
    ${csudo} systemctl daemon-reload
    
    #tarbitratord_service_config="${service_config_dir}/tarbitratord.service"
    #${csudo} bash -c "echo '[Unit]'                                  >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'Description=TDengine arbitrator service' >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'After=network-online.target'             >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'Wants=network-online.target'             >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo                                           >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo '[Service]'                               >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'Type=simple'                             >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'ExecStart=/usr/bin/tarbitrator'          >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'TimeoutStopSec=1000000s'                 >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'LimitNOFILE=infinity'                    >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'LimitNPROC=infinity'                     >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'LimitCORE=infinity'                      >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'TimeoutStartSec=0'                       >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'StandardOutput=null'                     >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'Restart=always'                          >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'StartLimitBurst=3'                       >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'StartLimitInterval=60s'                  >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo                                           >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo '[Install]'                               >> ${tarbitratord_service_config}"
    #${csudo} bash -c "echo 'WantedBy=multi-user.target'              >> ${tarbitratord_service_config}"
    #${csudo} systemctl enable tarbitratord

    if [ "$verMode" == "cluster" ]; then
        [ -f ${script_dir}/cfg/nginxd.service ] &&\
            ${csudo} cp ${script_dir}/cfg/nginxd.service \
            ${service_config_dir}/ || :
        ${csudo} systemctl daemon-reload

        #nginx_service_config="${service_config_dir}/nginxd.service"
        #${csudo} bash -c "echo '[Unit]'                                             >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'Description=Nginx For TDengine Service'             >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'After=network-online.target'                        >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'Wants=network-online.target'                        >> ${nginx_service_config}"
        #${csudo} bash -c "echo                                                      >> ${nginx_service_config}"
        #${csudo} bash -c "echo '[Service]'                                          >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'Type=forking'                                       >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'PIDFile=/usr/local/nginxd/logs/nginx.pid'           >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'ExecStart=/usr/local/nginxd/sbin/nginx'             >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'ExecStop=/usr/local/nginxd/sbin/nginx -s stop'      >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'TimeoutStopSec=1000000s'                            >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'LimitNOFILE=infinity'                               >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'LimitNPROC=infinity'                                >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'LimitCORE=infinity'                                 >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'TimeoutStartSec=0'                                  >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'StandardOutput=null'                                >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'Restart=always'                                     >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'StartLimitBurst=3'                                  >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'StartLimitInterval=60s'                             >> ${nginx_service_config}"
        #${csudo} bash -c "echo                                                      >> ${nginx_service_config}"
        #${csudo} bash -c "echo '[Install]'                                          >> ${nginx_service_config}"
        #${csudo} bash -c "echo 'WantedBy=multi-user.target'                         >> ${nginx_service_config}"
        if ! ${csudo} systemctl enable nginxd &> /dev/null; then
            ${csudo} systemctl daemon-reexec
            ${csudo} systemctl enable nginxd
        fi
        ${csudo} systemctl start nginxd
    fi
}

function install_taosadapter_service() {
    if ((${service_mod}==0)); then
        [ -f ${script_dir}/cfg/taosadapter.service ] &&\
            ${csudo} cp ${script_dir}/cfg/taosadapter.service \
            ${service_config_dir}/ || :
        ${csudo} systemctl daemon-reload
    fi
}

function install_service() {
    if ((${service_mod}==0)); then
        install_service_on_systemd
    elif ((${service_mod}==1)); then
        install_service_on_sysvinit
    else
        # must manual stop taosd
        kill_process taosd
    fi
}

vercomp () {
    if [[ $1 == $2 ]]; then
        return 0
    fi
    local IFS=.
    local i ver1=($1) ver2=($2)
    # fill empty fields in ver1 with zeros
    for ((i=${#ver1[@]}; i<${#ver2[@]}; i++)); do
        ver1[i]=0
    done

    for ((i=0; i<${#ver1[@]}; i++)); do
        if [[ -z ${ver2[i]} ]]
        then
            # fill empty fields in ver2 with zeros
            ver2[i]=0
        fi
        if ((10#${ver1[i]} > 10#${ver2[i]}))
        then
            return 1
        fi
        if ((10#${ver1[i]} < 10#${ver2[i]}))
        then
            return 2
        fi
    done
    return 0
}

function is_version_compatible() {

    curr_version=`ls ${script_dir}/driver/libtaos.so* | awk -F 'libtaos.so.' '{print $2}'`

    if [ -f ${script_dir}/driver/vercomp.txt ]; then
        min_compatible_version=`cat ${script_dir}/driver/vercomp.txt`
    else
        min_compatible_version=$(${script_dir}/bin/taosd -V | head -1 | cut -d ' ' -f 5)
    fi

    # [TD-5628] prompt to execute taosd --force-keep-file if upgrade from lower version within 2.0.16.0
    exist_version=$(/usr/local/taos/bin/taosd -V | head -1 | cut -d ' ' -f 3)
    vercomp $exist_version "2.0.16.0"
    case $? in
        2)
          prompt_force=1
          ;;
    esac

    vercomp $curr_version $min_compatible_version
    echo "" # avoid $? value not update

    case $? in
        0) return 0;;
        1) return 0;;
        2) return 1;;
    esac
}

function update_TDengine() {
    # Check if version compatible
    if ! is_version_compatible; then
        echo -e "${RED}Version incompatible${NC}"
        return 1
    fi

    # Start to update
    if [ ! -e taos.tar.gz ]; then
        echo "File taos.tar.gz does not exist"
        exit 1
    fi
    tar -zxf taos.tar.gz
    install_jemalloc
    #install_avro lib
    #install_avro lib64

    echo -e "${GREEN}Start to update TDengine...${NC}"
    # Stop the service if running
    if pidof taosd &> /dev/null; then
        if ((${service_mod}==0)); then
            ${csudo} systemctl stop taosd || :
        elif ((${service_mod}==1)); then
            ${csudo} service taosd stop || :
        else
            kill_process taosd
        fi
        sleep 1
    fi

    if [ "$verMode" == "cluster" ]; then
      if pidof nginx &> /dev/null; then
        if ((${service_mod}==0)); then
            ${csudo} systemctl stop nginxd || :
        elif ((${service_mod}==1)); then
            ${csudo} service nginxd stop || :
        else
            kill_process nginx
        fi
        sleep 1
      fi
    fi

    install_main_path

    install_log
    install_header
    install_lib
    if [ "$pagMode" != "lite" ]; then
      install_connector
    fi
    install_examples
    if [ -z $1 ]; then
        install_bin
        install_service
        install_taosadapter_service
        install_config
        install_taosadapter_config

        openresty_work=false
        if [ "$verMode" == "cluster" ]; then
            # Check if openresty is installed
            # Check if nginx is installed successfully
            if type curl &> /dev/null; then
                if curl -sSf http://127.0.0.1:${nginx_port} &> /dev/null; then
                    echo -e "\033[44;32;1mNginx for TDengine is updated successfully!${NC}"
                    openresty_work=true
                else
                    echo -e "\033[44;31;5mNginx for TDengine does not work! Please try again!\033[0m"
                fi
            fi
        fi

        #echo
        #echo -e "\033[44;32;1mTDengine is updated successfully!${NC}"
        echo
        echo -e "${GREEN_DARK}To configure TDengine ${NC}: edit /etc/taos/taos.cfg"
        if ((${service_mod}==0)); then
            echo -e "${GREEN_DARK}To start TDengine     ${NC}: ${csudo} systemctl start taosd${NC}"
        elif ((${service_mod}==1)); then
            echo -e "${GREEN_DARK}To start TDengine     ${NC}: ${csudo} service taosd start${NC}"
        else
            echo -e "${GREEN_DARK}To start TDengine     ${NC}: ./taosd${NC}"
        fi

        if [ ${openresty_work} = 'true' ]; then
            echo -e "${GREEN_DARK}To access TDengine    ${NC}: use ${GREEN_UNDERLINE}taos -h $serverFqdn${NC} in shell OR from ${GREEN_UNDERLINE}http://127.0.0.1:${nginx_port}${NC}"
        else
            echo -e "${GREEN_DARK}To access TDengine    ${NC}: use ${GREEN_UNDERLINE}taos -h $serverFqdn${NC} in shell${NC}"
        fi

        if ((${prompt_force}==1)); then
          echo ""
          echo -e "${RED}Please run 'taosd --force-keep-file' at first time for the exist TDengine $exist_version!${NC}"
        fi
        echo
        echo -e "\033[44;32;1mTDengine is updated successfully!${NC}"
    else
        install_bin
        install_config

        echo
        echo -e "\033[44;32;1mTDengine client is updated successfully!${NC}"
    fi

    rm -rf $(tar -tf taos.tar.gz)
}

function install_TDengine() {
    # Start to install
    if [ ! -e taos.tar.gz ]; then
      echo "File taos.tar.gz does not exist"
      exit 1
    fi
    tar -zxf taos.tar.gz

    echo -e "${GREEN}Start to install TDengine...${NC}"

    install_main_path

    if [ -z $1 ]; then
        install_data
    fi

    install_log
    install_header
    install_lib
    install_jemalloc
    #install_avro lib
    #install_avro lib64

    if [ "$pagMode" != "lite" ]; then
      install_connector
    fi
    install_examples

    if [ -z $1 ]; then # install service and client
        # For installing new
        install_bin
        install_service
        install_taosadapter_service

        openresty_work=false
        if [ "$verMode" == "cluster" ]; then
            # Check if nginx is installed successfully
            if type curl &> /dev/null; then
                if curl -sSf http://127.0.0.1:${nginx_port} &> /dev/null; then
                    echo -e "\033[44;32;1mNginx for TDengine is installed successfully!${NC}"
                    openresty_work=true
                else
                    echo -e "\033[44;31;5mNginx for TDengine does not work! Please try again!\033[0m"
                fi
            fi
        fi

        install_config

        # Ask if to start the service
        #echo
        #echo -e "\033[44;32;1mTDengine is installed successfully!${NC}"
        echo
        echo -e "${GREEN_DARK}To configure TDengine ${NC}: edit /etc/taos/taos.cfg"
        if ((${service_mod}==0)); then
          echo -e "${GREEN_DARK}To start TDengine     ${NC}: ${csudo} systemctl start taosd${NC}"
        elif ((${service_mod}==1)); then
          echo -e "${GREEN_DARK}To start TDengine     ${NC}: ${csudo} service taosd start${NC}"
        else
          echo -e "${GREEN_DARK}To start TDengine     ${NC}: taosd${NC}"
        fi

        #if [ ${openresty_work} = 'true' ]; then
        #     echo -e "${GREEN_DARK}To access TDengine    ${NC}: use ${GREEN_UNDERLINE}taos${NC} in shell OR from ${GREEN_UNDERLINE}http://127.0.0.1:${nginx_port}${NC}"
        #else
        #     echo -e "${GREEN_DARK}To access TDengine    ${NC}: use ${GREEN_UNDERLINE}taos${NC} in shell${NC}"
        #fi

        if [ ! -z "$firstEp" ]; then
            tmpFqdn=${firstEp%%:*}
            substr=":"
            if [[ $firstEp =~ $substr ]];then
                tmpPort=${firstEp#*:}
            else
                tmpPort=""
            fi
            if [[ "$tmpPort" != "" ]];then
                echo -e "${GREEN_DARK}To access TDengine    ${NC}: taos -h $tmpFqdn -P $tmpPort${GREEN_DARK} to login into cluster, then${NC}"
            else
                echo -e "${GREEN_DARK}To access TDengine    ${NC}: taos -h $tmpFqdn${GREEN_DARK} to login into cluster, then${NC}"
            fi
            echo -e "${GREEN_DARK}execute ${NC}: create dnode 'newDnodeFQDN:port'; ${GREEN_DARK}to add this new node${NC}"
            echo
        elif [ ! -z "$serverFqdn" ]; then
            echo -e "${GREEN_DARK}To access TDengine    ${NC}: taos -h $serverFqdn${GREEN_DARK} to login into TDengine server${NC}"
            echo
        fi

        echo -e "\033[44;32;1mTDengine is installed successfully!${NC}"
        echo
    else # Only install client
        install_bin
        install_config
        echo
        echo -e "\033[44;32;1mTDengine client is installed successfully!${NC}"
    fi

    touch ~/.taos_history
    rm -rf $(tar -tf taos.tar.gz)
}


## ==============================Main program starts from here============================
serverFqdn=$(hostname)
if [ "$verType" == "server" ]; then
    # Install server and client
    if [ -x ${bin_dir}/taosd ]; then
        update_flag=1
        update_TDengine
    else
        install_TDengine
    fi
elif [ "$verType" == "client" ]; then
    interactiveFqdn=no
    # Only install client
    if [ -x ${bin_dir}/taos ]; then
        update_flag=1
        update_TDengine client
    else
        install_TDengine client
    fi
else
    echo  "please input correct verType"
fi