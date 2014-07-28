#!/usr/bin/env python3
# encoding: utf-8
# === This file is part of Calamares - <http://github.com/calamares> ===
#
#   Copyright 2014, Teo Mrnjavac <teo@kde.org>
#
#   Calamares is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   Calamares is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with Calamares. If not, see <http://www.gnu.org/licenses/>.

import os
import shutil
import subprocess
import tempfile
from collections import namedtuple

from libcalamares import *
from filecopy import FileCopy

UnpackEntry = namedtuple( 'UnpackEntry', [ 'source', 'destination', 'sourceDir' ] )
UnpackStatusEntry = namedtuple( 'UnpackStatusEntry', [ 'copied', 'total' ] )

class UnsquashOperation:
    def __init__( self, unpack ):
        self.unpacklist = unpack
        self.unpackstatus = dict()
        for entry in unpack:
            self.unpackstatus[ entry.source ] = UnpackStatusEntry( copied=0, total=0 )


    def updateCopyProgress( self, source, nfiles ):
        if source in self.unpackstatus:
            self.unpackstatus[ source ].copied = nfiles
            self.reportProgress()


    def reportProgress( self ):
        progress = float( 0 )
        for statusEntry in self.unpackstatus:
            partialProgress = float( 0 )
            if statusEntry.total is not 0:
                partialProgress += 0.05
            else:
                continue

            partialProgress += 0.95 * ( statusEntry.copied / float( statusEntry.total ) )
            progress += partialProgress / len( self.unpackstatus )

        job.setprogress( progress )


    def run( self ):
        sourceMountPath = tempfile.mkdtemp()
        try:
            for entry in self.unpacklist:
                sqfsList = subprocess.check_output( [ "unsquashfs", "-l", entry.source ] )
                filesCount = sqfsList.splitlines().count()
                self.unpackstatus[ entry.source ].total = filesCount

                imgBaseName = os.path.splitext( os.path.basename( entry.source ) )[ 0 ]
                imgMountDir = sourceMountPath + os.sep + imgBaseName
                os.mkdir( imgMountDir )
                entry.sourceDir = imgMountDir
                self.reportProgress()
                self.unsquashImage( entry )
        finally:
            shutil.rmtree( sourceMountPath )


    def unsquashImage( self, entry ):
        subprocess.check_call( [ "mount", entry.source, entry.sourceDir, "-t", "squashfs", "-o", "loop" ] )
        try:
            t = FileCopy( entry.sourceDir, entry.destination, self.reportProgress )
            t.run()
        finally:
            subprocess.check_call( [ "umount", "-l", entry.sourceDir ] )



def run():
    # from globalStorage: rootMountPoint
    # from job.configuration:
    # the path to where to mount the source image(s) for copying
    # an ordered list of unpack mappings for sqfs file <-> target dir relative
    # to rootMountPoint, e.g.:
    # configuration:
    #     unpack:
    #         - source: "/path/to/squashfs/image.sqfs"
    #           destination: ""
    #         - source: "/path/to/another/image.sqfs"
    #           destination: ""

    rootMountPoint = globalStorage.value( "rootMountPoint" )
    if not rootMountPoint:
        return ( "No mount point for root partition in GlobalStorage",
                 "GlobalStorage does not contain a \"rootMountPoint\" key, doing nothing" )
    if not os.path.exists( rootMountPoint ):
        return ( "Bad mount point for root partition in GlobalStorage",
                 "GlobalStorage[\"rootMountPoint\"] is \"{}\", which does not exist, doing nothing".format( rootMountPoint ) )
    unpack = list()

    for entry in job.configuration[ "unpack" ]:
        source = os.path.abspath( entry[ "source" ] )
        destination = os.path.abspath( os.path.join( rootMountPoint, entry[ "destination" ] ) )

        if not os.path.isfile( source ) or not os.path.isdir( destination ):
            return ( "Bad source or destination",
                     "source=\"{}\"\ndestination=\"{}\"".format( source, destination ) )

        unpack.append( UnpackEntry( source, destination ) )

    unsquashop = UnsquashOperation( unpack )
    return unsquashop.run()