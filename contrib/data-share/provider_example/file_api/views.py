import json
import subprocess
import uuid

import magic
import sys

from django.conf import settings
from django.http import HttpResponse
# from rest_framework.parsers import FileUploadParser
from rest_framework.response import Response
from rest_framework.views import APIView
from rest_framework.parsers import MultiPartParser
import multiprocessing as mp
import transmissionrpc
import os.path
from django.http import HttpRequest

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

fh = logging.FileHandler(settings.LOG_FILE)
fh.setLevel(logging.DEBUG)

logFormatter = logging.Formatter("%(asctime)s [%(levelname)-5.5s]  %(message)s")
fh.setFormatter(logFormatter)

logger.addHandler(fh)


class UploadTorrentFile(APIView):
    """
    View to upload .torrent file to server

    """
    http_method_names = ('post',)

    # parser_classes = (FileUploadParser,)

    def post(self, request):
        logger.info('Got upload torrent file request')
        try:
            file_obj = request.FILES['file']
            logger.debug('got file {}'.format(file_obj))
        except:
            msg = 'Error reading file'
            logger.error(msg)
            return api_response(error=True, message=msg)

        mime_type = magic.from_buffer(file_obj.read(1024), mime=True)
        if mime_type not in ('application/octet-stream', 'application/x-bittorrent'):
            msg = 'File should be of application/octet-stream MIME type, not {}'.format(mime_type)
            logger.error(msg)
            return api_response(error=True, message=msg)

        # generated unique torrent name
        _uuid = uuid.uuid4()
        filename = '{}.torrent'.format(_uuid)
        file_path = '{}/{}'.format(settings.BITTORRENT_DOWNLOAD_DIR, filename)
        logger.debug('Generated unique torrent file name {} for {}'.format(filename, file_obj))
        logger.info('Saving file')
        # save file
        try:
            with open(file_path, 'wb+') as destination:
                for chunk in file_obj.chunks():
                    destination.write(chunk)
        except:
            msg = 'Could not save file'
            logger.error(msg + ' ' + file_obj)
            return api_response(error=True, message=msg)

        # add torrent transmission-remote- a
        add_torrent(file_path)
        msg = 'Torrent added'
        logger.debug('Success')

        return api_response(error=False, message=msg, file_id=_uuid)


class DownloadTorrentFile(APIView):
    """
    View to download .torrent file from server
    """
    http_method_names = ('get',)

    def get(self, request, file_uuid):
        filename = '{}.torrent'.format(file_uuid)
        x_forwarded_for = request.META.get('HTTP_X_FORWARDED_FOR')
        if x_forwarded_for:
            ip = x_forwarded_for.split(',')[0]
        else:
            ip = request.META.get('REMOTE_ADDR')
        logger.info('Download torrent {} request from {}'.format(file_uuid, ip))
        with open('{}/{}'.format(settings.BITTORRENT_DOWNLOAD_DIR, filename), 'rb') as source:
            response = HttpResponse(source)
        response['Content-Disposition'] = 'attachment; filename="{}"'.format(filename)

        return response


class MakeTorrent(APIView):
    http_method_names = ('post')
    parser_classes = (MultiPartParser,)

    def post(self, request, format=None):
        logger.info('Got make torrent request')
        # trying to get file
        try:
            file = request.FILES['file']
            logger.debug('Got file: {}'.format(file))
        except:
            msg = 'Could not read file or wrong key name, use \'file\' as key'
            logger.error(msg)
            return api_response(error=True, message=msg)

        # check the filename for wrong characters (required for transmission), and check if file with following
        # filename is already exist
        chars = set('()\'\"$%#@!&^* ')
        file_path = '{}/{}'.format(settings.FILE_PATH, file)
        if any((c in chars) for c in file._name) or os.path.isfile(file_path):
            msg = 'Invalid file name or file with following name exist'
            logger.error(msg)
            return api_response(error=True, message=msg)

        # upload file
        logger.debug('Saving file {} to {}'.format(file, settings.FILE_PATH))
        try:
            count_cores = mp.cpu_count()
            if count_cores > 8:
                # multiprocessing
                pool = mp.Pool(count_cores - 1)
                jobs = []
                with open(file_path, 'wb+') as destination:
                    for line in file:
                        jobs.append(pool.apply_async(destination.write, (line,)))

                for job in jobs:
                    job.get()
                pool.close()

            else:
                with open(file_path, 'wb+') as destination:
                    for chunk in file.chunks():
                        destination.write(chunk)

        except:
            msg = 'Could not save file'
            logger.error('Error saving file to {}'.format(settings.FILE_PATH))
            return api_response(error=True, message=msg)

        msg = []
        # make torrent
        _uuid = uuid.uuid4()
        torrent_file = '{}.torrent'.format(_uuid)
        torrent_path = '{}/{}'.format(settings.BITTORRENT_DOWNLOAD_DIR, torrent_file)
        logger.debug('Generated unique torrent name {} for {}'.format(torrent_file, file))
        logger.info('Creating torrent {}'.format(torrent_file))

        # cmd = 'transmission-create -o {} '.format(torrent_path)
        # for tr in settings.TRACKERS:
        #     cmd += '-t '+tr+' '
        # cmd += ' '+file_path
        cmd = 'transmission-create -o {} {} {}'.format(torrent_path, ''.join(['-t {} '.format(tr) for tr in settings.TRACKERS]), file_path)
        logger.debug('Executing command: {}'.format(cmd))

        try:
            p = subprocess.Popen([cmd], shell=True)
            p.wait()
            out, err = p.communicate()
            logger.debug('Popen create torrent return code: {}, output: {}'.format(p.returncode, out))
            if err:
                logger.error('Error in Popen: [returncode: {}] [error: {}]'.format(p.returncode, err.strip()))
                msg = 'Error while creating torrent file'
                return api_response(error=True, message=msg)

        except OSError as e:
            msg = 'Error while creating torrent file'
            logger.error('Error while executing torrent create command: ERRNO[{}] {}'.format(e.errno, e.strerror))
            return api_response(error=True, message=msg)
        except:
            logger.error('Error while executing torrent create command: {}'.format(sys.exc_info()[0]))
            msg = 'Error while creating torrent file'
            return api_response(error=True, message=msg)

        msg.append('Torrent created')
        # add torrent
        logger.info('Adding torrent {}'.format(torrent_file))
        add_torrent(torrent_path)

        msg.append('Torrent added')
        logger.debug('Success')
        return api_response(error=False, message=msg, file_id=_uuid)


def add_torrent(torrent_path):
    cmd = '{} \'{}\''.format(settings.BITTORRENT_TRACKER_ADD_TORRENT_CMD, torrent_path)
    try:
        p = subprocess.Popen([cmd], shell=True)
        p.wait()
        out, err = p.communicate()
        logger.debug('Popen add torrent return code: {}, output: {}'.format(p.returncode, out))
        if err:
            logger.error('Error in Popen: [returncode: {}] [error: {}]'.format(p.returncode, err.strip()))
            msg = 'Error while adding torrent file'
            return api_response(error=True, message=msg)
        if p.returncode == 1:
            logger.error('Error in Popen: [returncode: {}] [error: {}]'.format(p.returncode, err))
            msg = 'Error while adding torrent file'
            return api_response(error=True, message=msg)

    except OSError as e:
        msg = 'Error while adding torrent file'
        logger.error('Error while executing torrent add command: ERRNO[{}] {}'.format(e.errno, e.strerror))
        return api_response(error=True, message=msg)
    except:
        logger.error('Error while executing torrent add command: {}'.format(sys.exc_info()[0]))
        msg = 'Error while adding torrent file'
        return api_response(error=True, message=msg)


def api_response(error, message, file_id=None):
    if not error:
        # payload = {
        #     "status": {
        #         "message": message,
        #         "status": "SUCCESS",
        #         "messageid": "msg_103_info"
        #     },
        #     "content": {
        #         "url": "{}/file_api/download/{}/".format(settings.SITE_URL, file_id)
        #     }
        # }
        payload = {
            "result": "Success",
            "id": file_id,
            "url": "{}/file_api/download/{}/".format(settings.SITE_URL, file_id),
            "cost": 0
        }
    else:
        payload = {
            "status": {
                "message": message,
                "status": "ERROR",
                "messageid": "msg_9000_error"
            }
        }
    return Response(payload)