# Per-chip database (asyn ADDR 0..7). Driver maxAddr is 8.
# Unused addresses on 1- or 4-chip systems: DAC writes fail at HTTP; readbacks benign.
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP0,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP1,PORT=$(PORT),ADDR=1,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP2,PORT=$(PORT),ADDR=2,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP3,PORT=$(PORT),ADDR=3,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP4,PORT=$(PORT),ADDR=4,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP5,PORT=$(PORT),ADDR=5,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP6,PORT=$(PORT),ADDR=6,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP7,PORT=$(PORT),ADDR=7,TIMEOUT=1")
