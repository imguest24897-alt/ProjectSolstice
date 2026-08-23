#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 Nikita Travkin <nikita@trvn.ru>

# This script generates a pretty formatted version description using git VCS
# if it's available in the tree.
#
# The format is as follows:
#     <last tag>[-next-<commit>-<date>][-dirty]

# Return -dirty or empty string
git_dirty() {
	if [ -n "$(git status --porcelain)" ]
	then
		echo " \(dirty\)"
	fi
}

# The variable that basically lets the thing know that it's a alpha version idk im used to fish instead of sh or bash
export isAlpha=true
alphaVer() {
	if [ "$isAlpha" = true ];
	then
		echo "-alpha"
	fi
}

# Echo version of the project based on git
get_version_string() {
	if ! git rev-parse --is-inside-work-tree > /dev/null 2>&1
	then
		echo "$(date +%Y.%m.%d)$(alphaVer)"
		return
	fi

	head_rev=$(git rev-parse --short HEAD)
	head_date=$(git log -1 --format=%cd --date=format:"%Y%m%d")
	last_tag=$(git tag --sort=-taggerdate --merged | head -n1)
	if [ -z "$last_tag" ]
	then
		echo "$(date +%Y.%m.%d)$(alphaVer)$(git_dirty)"
		return
	fi

	last_tag_ref=$(git rev-list -n1 "$last_tag")
	if [ "$last_tag_ref" = "$(git rev-list -n1 HEAD)" ]
	then
		echo "$last_tag$(alphaVer)$(git_dirty)"
		return
	fi

	echo "$(date +%Y.%m.%d)$(alphaVer)$(git_dirty)"
}

get_version_string
