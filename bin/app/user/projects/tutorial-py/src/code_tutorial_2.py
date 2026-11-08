from code_tutorial_1 import CodeTutorial
from utility import uint

#------------------------------------------------------------------------------
#
# 5 - NO MORE CONTEXT
#  At this stage you see the entire file and no more context can be expanded.
#  For the final part of this introduction please have a look at where the
#  "unrelated_but_very_important()" function is used.
#
#------------------------------------------------------------------------------


def unrelated_but_very_important():
	pass


def calculate(problem):
	return problem()

#------------------------------------------------------------------------------
#
# 4 - MORE CONTEXT
#  Who would have expected such a simple question?
#  Note that expanding the view didn't change the active symbol. You can
#  proceed expanding the view until you hit the level of the file scope.
#  So go ahead and find out what else is hidden inside this file!
#
#------------------------------------------------------------------------------


def deep_thought():
	the_question = lambda: 6 * 7

	the_answer: uint = calculate(the_question)
	CodeTutorial.meaning_of_life_the_universe_and_everything = the_answer
