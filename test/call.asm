[bits 32]

extern exit

test:
    push $
    ret

global main
main:
    call test
    push 0
    call exit; 


