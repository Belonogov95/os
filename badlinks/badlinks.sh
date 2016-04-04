
go() {
    for s in `ls $1`
    do
        path=$1/$s 
        if test -d $path 
        then
            go $path
        else
            if test -h $path
            then
                realPath=$(readlink $path)
                if ! test -e $realPath
                    #echo slink not exist $path
                    tmr=$(stat --format "%Y" $path)
                    curTime=$(date +"%s")
                    diff=$((curTime - tmr))
                    oneWeek=$((60*60*24*7))

                    if [ "$oneWeek" -lt "$diff" ]
                    then
                        echo $path
                    fi
                fi
            fi 
        fi
    done
}



go $1






