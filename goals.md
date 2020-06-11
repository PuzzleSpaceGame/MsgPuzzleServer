#Objectives:
Puzzle Servers (PSs):
- Consume messages from a puzzle-specific work queue
- Return appropriate responses
- Listen on a local administrative queue
- Shutdown on command from local queue

Puzzle Server Managers (PSMs):
- Negotiate number and type of Puzzle Servers that 
    should be running with central server.
- Publish shutdown comands to local administrative queue
    when reducing capacity/shifting concerns.
    
