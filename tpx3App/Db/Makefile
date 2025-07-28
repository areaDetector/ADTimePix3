TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------
#  Add MACRO DEFINITION AFTER THIS LINE

#----------------------------------
#  Optimization of db files using dbst (default:NO)
#DB_OPT = YES

#-------------------------------------------
# Create and install (or just install) 
# databases, templates, subtitutions
DB += ADTimePix3.template
DB += TimePix3Base.template
DB += Chips.template
DB += File.template
DB += Server.template
DB += Measurement.template
DB += Dashboard.template
DB += MaskBPC.template
DB += OperatingVoltage.template

#-------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_TEMPLATE = <templatename>
include $(TOP)/configure/RULES
#----------------------------------------------
# ADD Rules after tis line.
