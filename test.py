a = 1
def foo(a, b):
	return a + b

def bar():
	print "hello in bar"
	return a + 5

if True:
	print foo(3, 5)
print bar()

if False:
	print "It's False."
False = 1
if False:
	print "False has been changed"
	print len("False become True")

# a = 1
# b = a + 1
# iris = b * 2
# ken = 3
# the_world_wide_name_is_really_long = 29
# if the_world_wide_name_is_really_long > ken:
# 	print 1133
# print a
# print b
# print iris
# print ken
# print the_world_wide_name_is_really_long


