if [-z "$RABBITMQ_SECRET"]; then
	echo "launching assets in Production Mode"
	for pls in build/logic_servers/*
       	do
		IFS='_' read -ra split <<< "$(basename $pls)"
		RABBITMQ_SECRET=$RABBITMQ_SECRET tmux new-session -d -s "$(basename $pls)" "$(echo $pls) -host 127.0.0.1 -port 5672 -queue st`echo $split` -user psserver -pass `echo $RABBITMQ_SECRET`"
	done
; else
	echo "launching in Development Mode"
	for pls in build/logic_servers/*; do
		IFS='_' read -ra split <<< "$(basename $pls)"
		tmux new-session -d -s "$(basename $pls)" "$(echo $pls) -host 127.0.0.1 -port 5672 -queue st`echo $split` -user psserver -pass fizzbuzz"
	done
; fi


