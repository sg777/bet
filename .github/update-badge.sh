#!/bin/bash
# Script to update README badge URL based on repository
REPO_URL=$(git remote get-url origin 2>/dev/null | sed 's/.*github.com[:/]\([^.]*\)\.git/\1/' | sed 's/\.git$//')
if [ -n "$REPO_URL" ]; then
  sed -i "s|https://github.com/[^/]*/bet/actions|https://github.com/$REPO_URL/actions|g" README.md
  echo "Updated badge to point to: $REPO_URL"
fi
