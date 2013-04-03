
#
# This is a command file.
#
# To make a new test, all you have to do is:
#   1.  Make a new directory under tests/
#   2.  Put a file like this (named "command") into that directory.
#
# The contents of this file will be passed to the shell (Bash or SSH),
# so any legal shell commands can go in this file.
# Or comments like this one, for that matter.
#
# Variable substiution is done using Python's printf format,
# meaning you need a percent sign, the variable name in parentheses,
# and the letter 's' (for 'string').
#
# Available variables include:
#   workdir     the directory where test input files have been copied,
#               and where test output files should end up.
#   minidir     the base directory where Rosetta software lives (rosetta/rosetta_source)
#   database    where the Rosetta database lives (rosetta/rosetta_database)
#   bin         where the Rosetta binaries live (rosetta/rosetta_source/bin)
#   binext      the extension on binary files, like ".linuxgccrelease"
#
# The most important thing is that the test execute in the right directory.
# This is especially true when we're using SSH to execute on other hosts.
# All command files should start with this line:
#

cd /Users/rhiju/src/rosetta/rosetta_tests/integration/new/swa_protein_loop_sampler


[ -x /Users/rhiju/src/rosetta/rosetta_source/bin/swa_protein_main.default.macosgccrelease ] || exit 1
/Users/rhiju/src/rosetta/rosetta_source/bin/swa_protein_main.default.macosgccrelease @flags -database /Users/rhiju/src/rosetta/rosetta_database -run:constant_seed -nodelay  2>&1 \
    | egrep -vf ../../ignore_list \
    > log


#
# After that, do whatever you want.
# Files will be diffed verbatim, so if you want to log output and compare it,
# you'll need to filter out lines that change randomly (e.g. timings).
# Prefixing your tests with "nice" is probably good form as well.
# Don't forget to use -constant_seed -nodelay  so results are reproducible.
# Here's a typical test for a Mini binary, assuming there's a "flags" file
# in this directory too:
#
## /Users/rhiju/src/rosetta/rosetta_source/bin/MY_MINI_PROGRAM.default.macosgccrelease @flags -database /Users/rhiju/src/rosetta/rosetta_database -run:constant_seed -nodelay  2>&1 \
##     | egrep -v 'Finished.+in [0-9]+ seconds.' \
##     | egrep -v 'Dunbrack library took .+ seconds to load' \
##     > log
#
# Or if you don't care whether the logging output changes:
#
## /Users/rhiju/src/rosetta/rosetta_source/bin/MY_MINI_PROGRAM.default.macosgccrelease @flags -database /Users/rhiju/src/rosetta/rosetta_database -run:constant_seed -nodelay  2>&1 \
##     > /dev/null
#