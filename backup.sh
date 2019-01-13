set -eu

rar=rar
if ! type rar 2>/dev/null; then
    rar="/cygdrive/C/Program Files/WinRAR/Rar.exe"
fi

if [ ! -f "$rar" ]; then
    echo "Rar not found!"
    exit 1
fi

date="$(date +'%d.%m.%Y.%-H')"
if [[ ! $date =~ ^([^.]+)\.([^.]+)\.([^.]+)\.(.*)$ ]]; then
    echo "Can't determine the date."
    exit 1
fi

day=${BASH_REMATCH[1]}
month=${BASH_REMATCH[2]}
year=${BASH_REMATCH[3]}
hour=${BASH_REMATCH[4]}

if (( $hour < 13 )); then
    date="$(date -d yesterday +'%d.%m.%Y')"
else
    date=$day.$month.$year
fi

destFile='swos-port-'$date'.rar'

# check for argument destination directory, if not, use preset (first env. then command line)
destDir=${1:-"/cygdrive/D/backup/swos-port/"}
destDir=${SWOS_PORT_BACKUP_DIR:-${destDir}}
destDir=${destDir%/}/

if  [ ! -d $destDir ]; then
    echo "Destination directory $destDir doesn't exist"
    exit 1
fi

# check if dest file already exists and add suffix if necessary
suffix=0
destPath=$destDir$destFile

while [ -f $destPath ]
do
    #((suffix++))
    suffix=$((suffix+1))

    if (( suffix > 99 )); then
        echo "Too many backup files for a single date, giving up."
        exit 1
    fi

    seq=$(seq -f "%02g" $suffix $suffix)
    destPath=$destDir$destFile
    destPath=${destPath/.rar/-$seq.rar}
done

destPath=$(cygpath -w $destPath)
echo "Creating archive: $destPath"
"$rar" a -r -m5 -s -- "$destPath" 'doc/*' 'ida2asm/*' 'src/*' 'swos/*' 'vc-proj/*.sln' 'vc-proj/*.vcxproj' \
    'vc-proj/*.user' 'vc-proj/*.filters' vc-proj/Types.natvis backup.sh
