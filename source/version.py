# This function expects that the current working directory is the Rosetta root directory.
# If that's ever not true, we need to modify this to take an optional dir name on the cmd line.
# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org. Questions about this can be
# (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

# /// @file   version.py
# ///
# /// @brief
# /// @author Ian W. Davis
# /// @author Andrew Leaver-Fay
# /// @author Sergey Lyskov

import sys, time, os, re, os.path, commands


def svn_version():
    '''
    Generates a C++ header file with a summary of the current version(s) of the working copy, if any.
    If this code is not a git repository, the version will be given as "exported".
    Although this is being placed in core/, it doesn't really belong to any subproject.
    There's no good way to know when the version summary will change, either, so we just generate the file every time.
    '''
    rootdir = os.getcwd()
    while os.path.basename(rootdir) and os.path.basename(rootdir) != 'main':
        rootdir = os.path.dirname(rootdir) # Walk up directory tree to "main" directory, or root of filesystem.

    #Check if we're in a git repository - if not, skip gracefully (so git doesn't throw warnings in releases.)
    if os.path.exists(os.path.join( rootdir, ".git" )):
        ver = os.popen("git rev-parse HEAD").read().strip() or "unknown"
        if ver != "unknown":
            url = os.popen("git remote -v |grep fetch |awk '{print $2}'|head -n1").read().strip()
            if url == "":
                url = "unknown"
        else:
            url="unknown"

        commit_date = os.popen("git log %s -1 --format='%%ci'" % ver).read().strip()  #[:-6]
    else:
        # We're probably a release version
        ver = "exported"
        url = "http://www.rosettacommons.org"
        commit_date = ""

    # get_commit_id.sh is not in the standard repository, but is added by PyRosetta?
    # See commit log for  dbbff5655669f41af0dfa7c9421fc89e36b2a227
    commit_id = 'unknown'
    if os.path.isfile('get_commit_id.sh'):
        (res, output) = commands.getstatusoutput('./get_commit_id.sh %s' % ver)
        print 'Asked Testing server for commit id, got reply:', repr(output)
        if (res  or  not output  or not output.isdigit() ): commit_id = 'failed_to_get_id' # simple validation
        else: commit_id = str(int(output))

    if commit_id != 'unknown': ver = commit_id + ':' + ver

    file( os.path.normpath("src/devel/svn_version.cc"), "w" )          .write( version_cc_template % vars())
    file( os.path.normpath("src/python/bindings/src/version.py"), "w" ).write( version_py_template % vars())


def main():
    # Run with timing
    starttime = time.time()
    sys.stdout.write("Running versioning script ... ")
    sys.stdout.flush() # Make sure it gets dumped before running the function.
    svn_version()
    sys.stdout.write("Done. (%.1f seconds)\n" % (time.time() - starttime) )


version_py_template = '''\
commit_id = '%(commit_id)s'
commit    = '%(ver)s'
url       = '%(url)s'
date      = '%(commit_date)s'
'''

version_cc_template = '''// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   devel/svn_version.cc
///
/// @brief
/// @author Ian W. Davis
/// @author Andrew Leaver-Fay
/// @author Sergey Lyskov

/*****************************************************************************
*   This file is automatically generated by the build system.                *
*   DO NOT try modifying it by hand -- your changes will be lost.            *
*   DO NOT commit it to the Git repository.                                  *
*****************************************************************************/

#include <core/svn_version.hh>

namespace devel {

std::string rosetta_svn_version() { return "%(ver)s %(commit_date)s"; }
std::string rosetta_svn_url() { return "%(url)s"; }

class VersionRegistrator
{
public:
	VersionRegistrator() {
		core::set_svn_version_and_url( rosetta_svn_version(), rosetta_svn_url() );
	}
};

// There should only ever be one instance of this class
// so that core::set_svn_version_and_url is called only once
VersionRegistrator vr;

void
register_version_with_core() {
	// oh -- there's nothing in this function.  But
	// forcing devel::init to call this function ensures
	// that the vr variable in this file gets instantiated
}

} // namespace devel
'''

if __name__ == "__main__" or __name__ == "__builtin__":
    main()
