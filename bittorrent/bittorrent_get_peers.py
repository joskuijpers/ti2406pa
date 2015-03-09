#!/usr/bin/env python

__version__ = "1.1"
__author__  = "Pawel Garbacki <pawelg@gmail.com> and Lucia D'Acunto"

from sys import argv, exit
from bencode import bencode, bdecode
from binascii import a2b_hex,b2a_hex
from hashlib import sha1
from urllib import quote,urlopen, urlencode,unquote
from os.path import basename
from socket import *

# connects to tracker specified by <code>url</code> and retrieves a list of 20 
# random peers
#
def get_random_peers(url):
    try:
        handle = urlopen(full_url)
        raw_response = handle.read()
        handle.close()
        response = bdecode(raw_response)
        peers = response.get('peers')
        random_peers = []
        if type(peers) == type(''):
            for peer in xrange(0, len(peers), 6):
                ip = '.'.join([str(ord(i)) for i in peers[peer:peer+4]])
                port = (ord(peers[peer+4]) << 8) | ord(peers[peer+5])
                random_peers.append((ip, port))
        else:
            for peer in peers:
                random_peers.append([peer.get('ip'), "%s" % peer.get('port')])
        return random_peers
    except TypeError:
        print "Result Failed Type error."
        exit(2)
    except IOError:
        print "Result Failed I/O Exception occurred. Check URL."
        exit(2)
    except:
        raise

# print out to screen
#
def dump_peers(torrent_file_name, peers):
    for peer in peers:
        print peer
    
# respresents the torrent file meta data
#
class FileMeta:
# extracts meta data from a torrent file
#
    def __init__(self, torrent_file_name):
        torrent_file = open(torrent_file_name, 'rb')
        meta = bdecode(torrent_file.read())
        self.file_name = torrent_file_name
        self.announce = meta['announce']
        info = meta['info']
        self.info_hash = sha1(bencode(info))
        self.piece_length = info['piece length']
        self.n_pieces = len(info['pieces']) / 20
        if info.has_key('length'):
            # let's assume we have a single file
    #       file_length = info['length']
            self.length = info['length']
        else:
            # let's assume we have a directory structure
            file_length = 0;
            for file in info['files']:
                file_length += file['length']
            self.length = file_length
        dummy, self.last_piece_length = divmod(self.length, self.piece_length)
    #   piece_number, last_piece_length = divmod(file_length, piece_length)

#
# parses a torrent file to obtain the identity of a tracker, requests from the
# tracker 20 random peers and sends those peers' identities to the buffer
#

if __name__ == '__main__':
    if (len(argv) != 2):
        print "Usage: getpeers <torrent file>"
        exit(2)
    #init() # initialize the system
    torrent_file_name = argv[1]
    file_meta = FileMeta(torrent_file_name)
    full_url = file_meta.announce + '?info_hash=' + quote(a2b_hex(file_meta.info_hash.hexdigest())) + '&peer_id=R520-----e7YcWsHfZfF&port=7760&uploaded=0&downloaded=0&left=' + str(file_meta.length) + '&no_peer_id=1&compact=1&event=started&key=iw10.t'
    random_peers = get_random_peers(full_url)
    dump_peers(basename(torrent_file_name), random_peers)
