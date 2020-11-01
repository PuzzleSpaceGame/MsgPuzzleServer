./build/bridges/bridges_logic_server -host 127.0.0.1 -port 5672 -queue test -user guest -pass guest &
python3 msgtest.py 
