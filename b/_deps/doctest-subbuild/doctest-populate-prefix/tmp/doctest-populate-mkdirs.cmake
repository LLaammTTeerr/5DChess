# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/lamter/5DChess/.claude/worktrees/agent-abbcc5aa09f651e16/b/_deps/doctest-src")
  file(MAKE_DIRECTORY "/home/lamter/5DChess/.claude/worktrees/agent-abbcc5aa09f651e16/b/_deps/doctest-src")
endif()
file(MAKE_DIRECTORY
  "/home/lamter/5DChess/.claude/worktrees/agent-abbcc5aa09f651e16/b/_deps/doctest-build"
  "/home/lamter/5DChess/.claude/worktrees/agent-abbcc5aa09f651e16/b/_deps/doctest-subbuild/doctest-populate-prefix"
  "/home/lamter/5DChess/.claude/worktrees/agent-abbcc5aa09f651e16/b/_deps/doctest-subbuild/doctest-populate-prefix/tmp"
  "/home/lamter/5DChess/.claude/worktrees/agent-abbcc5aa09f651e16/b/_deps/doctest-subbuild/doctest-populate-prefix/src/doctest-populate-stamp"
  "/home/lamter/5DChess/.claude/worktrees/agent-abbcc5aa09f651e16/b/_deps/doctest-subbuild/doctest-populate-prefix/src"
  "/home/lamter/5DChess/.claude/worktrees/agent-abbcc5aa09f651e16/b/_deps/doctest-subbuild/doctest-populate-prefix/src/doctest-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/lamter/5DChess/.claude/worktrees/agent-abbcc5aa09f651e16/b/_deps/doctest-subbuild/doctest-populate-prefix/src/doctest-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/lamter/5DChess/.claude/worktrees/agent-abbcc5aa09f651e16/b/_deps/doctest-subbuild/doctest-populate-prefix/src/doctest-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
