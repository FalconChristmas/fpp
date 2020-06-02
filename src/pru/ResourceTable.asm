    .global    ||pru_remoteproc_ResourceTable||
    .sect    ".resource_table:retain", RW
    .retain
    .align    1
    .elfsym    ||pru_remoteproc_ResourceTable||,SYM_SIZE(20)
||pru_remoteproc_ResourceTable||:
    .bits    1,32
    .bits    0,32
    .bits    0,32
    .bits    0,32
    .bits    0,32
