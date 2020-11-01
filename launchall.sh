for pls in build/logic_servers/*; do
	IFS='_' read -ra split <<< "$(basename $pls)"
	tmux new-session -d -s "$(basename $pls)" "$(echo $pls) -host 127.0.0.1 -port 5672 -queue st$( echo ${split[0]}) -user psserver -pass fizzbuzz"
done

