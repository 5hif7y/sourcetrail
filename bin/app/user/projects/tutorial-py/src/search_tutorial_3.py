from utility import uint

#------------------------------------------------------------------------------
#
# 3 - FUZZY MATCHING
#  While looking for matching symbols the search bar does not only consider
#  strict matches to the entered substring. Instead it uses a more fuzzy
#  approach. This is really helpful when you only have a rough idea of what the
#  element you are really looking for is called.
#  While showing the list of results the autocompletion also display the kind
#  of each of the results to the very right.
#  Try to activate the class with the name "NameConflict" next.
#
#------------------------------------------------------------------------------


def autocompletion_example():
	results: uint = 0
