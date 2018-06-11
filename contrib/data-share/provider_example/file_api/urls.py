from django.urls import path

from file_api.views import DownloadTorrentFile
from file_api.views import UploadTorrentFile
from file_api.views import MakeTorrent
# from bittx.views import GetUnspentTransactions
# from bittx.views import PrepareUnsignedTransaction
# from bittx.views import SendSignedTransaction


app_name = 'file_api'
urlpatterns = [
    path(r'upload_file/', UploadTorrentFile.as_view(), name='upload_file'),
    path(r'download/<uuid:file_uuid>/', DownloadTorrentFile.as_view(), name='upload_file'),
    path(r'make_torrent/', MakeTorrent.as_view(), name='make_torrent')
    # url(r'^get_unspent_transactions/$', GetUnspentTransactions.as_view(),
    #     name='get_unspent_txs'),
    # url(r'^prepare_unsigned_transaction/$', PrepareUnsignedTransaction.as_view(),
    #     name='prepare_unsigned_tx'),
    # url(r'^send_signed_transaction/$', SendSignedTransaction.as_view(),
    #     name='send_signed_tx'),
]
