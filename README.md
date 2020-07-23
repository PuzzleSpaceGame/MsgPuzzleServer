Messages to the PuzzleLogicServer:
- 6 character type, followed by hyphen-delimited options
    - KILLXX : kills server
    - GETCFG : gets a json describing the config options
    - NEWXXX-[comma-delimited options] : makes a new game responds with draw commands and a current game state
    - REDRAW-[gamestate] : returns draw commands for a gamestate
    - UPDATE-[gamestate]-x,y,b : updates game with a user input
