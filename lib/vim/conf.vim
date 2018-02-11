syn keyword	confTodo	contained FIX

syn clear	confString
syn region	confString	start=+\W"\|\W'+ms=s+1 skip=+\\"\|\\'+ end=+"\|'+ oneline
