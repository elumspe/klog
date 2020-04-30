#include "lotwutilities.h"
#include <QCoreApplication>
#include <QUrl>
#include <QNetworkRequest>
#include <QFile>
//#include <QDebug>

LoTWUtilities::LoTWUtilities(const QString &_klogDir, const QString &_klogVersion, const QString &_parentFunction, DataProxy_SQLite *dp)
{
     //qDebug() << "LoTWUtilities::LoTWUtilities(): " << _klogDir << endl;
    dataProxy = dp;
    util = new Utilities;    
    manager = new QNetworkAccessManager(this);

    reply = nullptr;
    file = new QFile;
    //url = new QUrl;
    klogDir = _klogDir;
    klogVersion = _klogVersion;
    downloadAborted = false;
    stationCallsign.clear();
    startDate.clear();
    lotwQuery.clear();
    lotwUser.clear();
    lotwPassword.clear();
    fileName = "lotwDownload.adi";

    pDialog = new QProgressDialog(nullptr);
    pDialog->cancel();
     //qDebug() << "LoTWUtilities::LoTWUtilities(): - END"  << endl;
}

LoTWUtilities::~LoTWUtilities()
{
      //qDebug() << "LoTWUtilities::~LoTWUtilities"  << endl;
}

void LoTWUtilities::setFileName(const QString &_fn)
{
    //qDebug() << "LoTWUtilities::setFileName: " << _fn  << endl;
    if (_fn.length()>0)
    {
        fileName = _fn;
    }
    //qDebug() << "LoTWUtilities::setFileName - END"   << endl;
}

QString LoTWUtilities::getFileName()
{
    //qDebug() << "LoTWUtilities::getFileName: " << fileName   << endl;
    return fileName;
}

bool LoTWUtilities::selectQuery(const int _queryId)
{
     //qDebug() << "LoTWUtilities::selectQuery: - Start: " << QString::number(_queryId) << endl;
    bool savePassword = true;
    if (lotwPassword.length()<1)
    {
        savePassword = false;

        bool ok;
        lotwPassword = QInputDialog::getText(nullptr, tr("KLog - LoTW password needed"),
                                                   tr("Please enter your LoTW password: "), QLineEdit::Password, "", &ok);
        if (!ok)
        {
             //qDebug() << "LoTWUtilities::selectQuery: - END 1" <<  endl;
            return false;
        }
    }
    lotwQuery = QString("https://lotw.arrl.org/lotwuser/lotwreport.adi?login=%1&password=%2&qso_query=1&qso_qsl=no&qso_owncall=%3&qso_startdate=%4").arg(lotwUser).arg(lotwPassword).arg(stationCallsign).arg(startDate);
    if (!savePassword)
    {// We delete the password as soon as possible if the user is not willing to save it
        lotwPassword = QString();
    }
    url = QUrl(lotwQuery);

     //qDebug() << "LoTWUtilities::selectQuery: - END" <<  endl;

    return true;
}

bool LoTWUtilities::setStationCallSign (const QString &_call)
{
     //qDebug() << "LoTWUtilities::setStationCallSign: " << _call << endl;
    if (!util->isValidCall(_call))
    {
         //qDebug() << "LoTWUtilities::setStationCallSign: FALSE "  << endl;
        return false;
    }
    if (((dataProxy->getStationCallSignsFromLog(-1)).contains(_call)))
    {
         //qDebug() << "LoTWUtilities::setStationCallSign: TRUE"  << endl;
        stationCallsign = _call;
        QDate date = dataProxy->getFirstQSODateFromCall(stationCallsign);
         //qDebug() << "LoTWUtilities::setStationCallSign: Date: " << startDate  << endl;        
        //date = QDate::fromString(startDate, "yyyy/MM/dd");
        if (date.isValid())
        {
            startDate = date.toString("yyyyMMdd");
             //qDebug() << "LoTWUtilities::setStationCallSign: StartDate" << startDate  << endl;
        }
        else
        {
            startDate.clear();
             //qDebug() << "LoTWUtilities::setStationCallSign: StartDate not valid Date" << endl;
            return false;
        }

         //qDebug() << "LoTWUtilities::setStationCallSign: startDate: " << startDate  << endl;
        return true;
    }
      //qDebug() << "LoTWUtilities::setStationCallSign: FALSE 2"  << endl;
    return false;
}

void LoTWUtilities::startRequest(QUrl url)
{
    //qDebug() << "LoTWUtilities::startRequest: " << url.toString()  << endl;
    QByteArray agent = QString("KLog-" + klogVersion).toUtf8();
    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, agent);
    //request.setRawHeader("User-Agent", agent);
    //reply = manager->get(QNetworkRequest(url));
    reply = manager->get(request);
    //qDebug() << "LoTWUtilities::startRequest - 10"  << endl;
      // Whenever more data is received from the network,
      // this readyRead() signal is emitted
      connect(reply, SIGNAL(readyRead()), this, SLOT(slotReadyRead()));

      // Also, downloadProgress() signal is emitted when data is received
      connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(slotDownloadProgress(qint64)));

      // This signal is emitted when the reply has finished processing.
      // After this signal is emitted,
      // there will be no more updates to the reply's data or metadata.
      connect(reply, SIGNAL(finished()), this, SLOT(slotFinished()));
       //qDebug() << "LoTWUtilities::startRequest: - END" << endl;
}

int LoTWUtilities::download()
{
    //qDebug() << "LoTWUtilities::download - Start" << endl;
    if (!selectQuery(-1))
    {
        //qDebug() << "LoTWUtilities::download - END-1" << endl;
        return -1;
    }
    //qDebug() << "LoTWUtilities::download: - 10" << endl;
    QFileInfo fileInfo(url.path());

    //qDebug() << "LoTWUtilities::download: - 11" << endl;
    if (QFile::exists(fileName))
    {
        //qDebug() << "LoTWUtilities::download: - 12" << endl;
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setWindowTitle(tr("KLog - File exist"));
        QString aux = QString(tr("There is a file already existing with the name that will be used.") );
        msgBox.setText(tr("The file %1 already exist. Do you want to overwrite?").arg(fileName));
        msgBox.setDetailedText(aux);
        msgBox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);
        int ret = msgBox.exec();
        //qDebug() << "LoTWUtilities::download: - 13" << endl;
        if (ret == QMessageBox::No)
        {
             //qDebug() << "LoTWUtilities::download - END- 2" << endl;
            return -2;
        }
        //qDebug() << "LoTWUtilities::download: - 14" << endl;
        //file->close();
        //file->remove(fileName);
        //QFile::remove(fileName);
        //qDebug() << "LoTWUtilities::download: - 15" << endl;
      }
        //qDebug() << "LoTWUtilities::download: - 20 Filename: -" << fileName << "-" << endl;
        if (!file->isOpen())
        {
            //qDebug() << "LoTWUtilities::download: - 21 Filename: -" << endl;
            file->setFileName(fileName);
            //qDebug() << "LoTWUtilities::download: - 22 Filename: -" << endl;
        }
        //qDebug() << "LoTWUtilities::download: - 23 Filename: -" << endl;

      if (!file->open(QIODevice::WriteOnly))
      {
          QMessageBox msgBox;
          msgBox.setIcon(QMessageBox::Warning);
          msgBox.setWindowTitle(tr("KLog - Can't write the file"));
          QString aux = QString(tr("KLog was not able to save the file %1.\nError returned: %2") ).arg(fileName).arg(file->errorString());
          msgBox.setText(tr("The file already %1 exist.").arg(fileName));
          msgBox.setDetailedText(aux);
          msgBox.setStandardButtons(QMessageBox::Ok);
          msgBox.setDefaultButton(QMessageBox::Ok);
          msgBox.exec();
          //file->close();
          //delete file;
          //file = nullptr;
           //qDebug() << "LoTWUtilities::download - END - 3" << endl;
          return -3;
      }
    //qDebug() << "LoTWUtilities::download: - 30" << endl;
      // used for progressDialog
      // This will be set true when canceled from progress dialog
    downloadAborted = false;
    //qDebug() << "LoTWUtilities::download: - 31" << endl;
    //progressDialog = new QProgressDialog(nullptr);
    //qDebug() << "LoTWUtilities::download: - 40" << endl;
    pDialog->setLabelText(tr("Downloading data to file: %1.").arg(fileName));
    //qDebug() << "LoTWUtilities::download: - 41" << endl;
    pDialog->setWindowTitle(tr("KLog - LoTW download"));
    //qDebug() << "LoTWUtilities::download: - 42" << endl;
    pDialog->setWindowModality(Qt::WindowModal);
    //qDebug() << "LoTWUtilities::download: - 43" << endl;
    pDialog->reset();
    //qDebug() << "LoTWUtilities::download: - 44" << endl;
    pDialog->setRange(0, 0);
    //qDebug() << "LoTWUtilities::download: - 45" << endl;
    pDialog->setMinimumDuration(0);
    //qDebug() << "LoTWUtilities::download: - 46" << endl;
    pDialog->show();
    //qDebug() << "LoTWUtilities::download: - 47" << endl;

    connect(pDialog, SIGNAL(canceled()), this, SLOT(slotCancelDownload()));
    //qDebug() << "LoTWUtilities::download: - 50" << endl;

    startRequest(url);
     //qDebug() << "LoTWUtilities::download - END" << endl;
    return 1;

}

void LoTWUtilities::slotDownloadProgress(qint64 bytesRead) {
    //qDebug() << "LoTWUtilities::slotDownloadProgress: " << QString::number(bytesRead) << "/" << QString::number(totalBytes) << endl;
    if (downloadAborted)
    {
         //qDebug() << "LoTWUtilities::slotDownloadProgress: CANCELLED" << endl;
        return;
    }

    pDialog->setValue(bytesRead);
     //qDebug() << "LoTWUtilities::slotDownloadProgress - END " << endl;
}

void LoTWUtilities::slotReadyRead()
{
    //qDebug() << "LoTWUtilities::slotReadyRead: " << reply->readLine() << endl;
    if (file)
    {
        file->write(reply->readAll());
    }
    //qDebug() << "LoTWUtilities::slotReadyRead - END" << endl;
}

void LoTWUtilities::slotFinished()
{
     //qDebug() << "LoTWUtilities::slotFinished - Started" << endl;
    // when canceled
     if (downloadAborted)
     {
         if (file)
         {
             file->close();
             file->remove();
             //delete file;
             //file = nullptr;
         }
         //reply->deleteLater();
         pDialog->cancel();
         reply->close();
         //qDebug() << "LoTWUtilities::slotFinished - END Canceled" << endl;
         return;
     }

     // download finished normally
    pDialog->cancel();
    file->flush();
    file->close();

     // get redirection url
     QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
     if (reply->error())
     {
        file->remove();
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(tr("KLog - Download error"));
        QString aux = QString(tr("The downloading error is: %1") ).arg(reply->errorString());
        msgBox.setText(tr("There was an error while downloading the file from LoTW."));
        msgBox.setDetailedText(aux);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
    }
    else if (!redirectionTarget.isNull())
    {
        QUrl newUrl = url.resolved(redirectionTarget.toUrl());
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setWindowTitle(tr("KLog - Redirection found"));
        QString aux = QString(tr("The remote server redirected our connection to %1") ).arg(newUrl.toString());
        msgBox.setText(tr("Do you want to follow the redirection?"));
        msgBox.setDetailedText(aux);
        msgBox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);
        int ret = msgBox.exec();
        if (ret == QMessageBox::Yes)
        {
            url = newUrl;
            //reply->deleteLater();
            file->open(QIODevice::WriteOnly);
            file->resize(0);
            startRequest(url);
             //qDebug() << "LoTWUtilities::slotFinished - END-1" << endl;
            return;
        }
     }
    else
    {
        //QString fileName = QFileInfo(QUrl(ui->urlEdit->text()).path()).fileName();
         //ui->statusLabel->setText(tr("Downloaded %1 to %2.").arg(fileName).arg(QDir::currentPath()));
         //ui->downloadButton->setEnabled(true);
         //qDebug() << "LoTWUtilities::slotFinished:  "  << endl;
     }

    //reply->deleteLater();


    parseDownloadedFile(file->fileName());
    //delete file;
    //file = nullptr;


      //qDebug() << "LoTWUtilities::slotReadyRead - END" << endl;
}

void LoTWUtilities::slotCancelDownload()
{
     //qDebug() << "LoTWUtilities::slotCancelDownload - Start" << endl;
    downloadAborted = true;
    reply->abort();
     //qDebug() << "LoTWUtilities::slotCancelDownload - END" << endl;
}

void LoTWUtilities::setUser(const QString &_call)
{
     //qDebug() << "LoTWUtilities::setUser: " << _call << endl;
    lotwUser = _call;
     //qDebug() << "LoTWUtilities::setUser: END" << endl;
}

void LoTWUtilities::setPass(const QString &_pass)
{
     //qDebug() << "LoTWUtilities::setPass: " << _pass << endl;
    lotwPassword = _pass;
     //qDebug() << "LoTWUtilities::setPass: END" << endl;
}

bool LoTWUtilities::getIsReady()
{
     //qDebug() << "LoTWUtilities::getIsReady: user/station: -" << lotwUser <<"/" << stationCallsign << "-" << endl;
    if ((lotwUser.length()>1) && (stationCallsign.length()>1))
    {
         //qDebug() << "LoTWUtilities::getIsReady: true" << endl;
        return true;
    }
    else
    {
         //qDebug() << "LoTWUtilities::getIsReady: false" << endl;
      return false;
    }

}

void LoTWUtilities::parseDownloadedFile(const QString &_fn)
{
     //qDebug() << "LoTWUtilities::parseDownloadedFile: " << _fn << endl;
    QString _fileName = _fn;
    QMessageBox msgBox;
    QString aux;

    QFile file( _fileName );
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
         //qDebug() << "LoTWUtilities::parseDownloadedFile File not found" << _fileName << endl;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(tr("KLog - File not found"));
        msgBox.setText(tr("KLog can't find the downloaded file."));
        aux = QString(tr("It was not possible for find the file %1 that has been just downloaded.") ).arg(_fn);

        msgBox.setDetailedText(aux);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
        //qDebug() << "LoTWUtilities::parseDownloadedFile: END-1" << endl;
        return ;
    }
    else
    {
        //qint64 startOfFile = file.pos();
        // Look for a Header
        bool hasHeader = false;
        int numQSO = 0;
        QString Lotw_owncall = QString("OWNCALL: %1").arg(stationCallsign.toUpper());
        //bool hasOwnCall = false;
        //bool hasProgramID = false;
        bool userPasswordError = false;
        while ((!file.atEnd()) && (!hasHeader))
        {
            QByteArray line = file.readLine();
            QString lineU = line.toUpper();
            //qDebug() << "LoTWUtilities::parseDownloadedFile: lineU: " << lineU << endl;
            if (lineU.contains("<EOH>"))
            {
                 //qDebug() << "LoTWUtilities::parseDownloadedFile: EOH found" << endl;
                hasHeader = true;
            }
            //else if (lineU.contains("<PROGRAMID:4>LOTW"))
            //{
            //     //qDebug() << "LoTWUtilities::parseDownloadedFile: ProgramID found" << endl;
            //    hasProgramID = true;
            //}
            //else if (lineU.contains(Lotw_owncall))
            //{
            //     //qDebug() << "LoTWUtilities::parseDownloadedFile: OWNCALL found" << endl;
            //    hasOwnCall = true;
            //}
            else if (lineU.contains("<APP_LOTW_NUMREC:"))
            {
                QStringList data;
                data << lineU.split('>', QString::SkipEmptyParts);
                numQSO = (data.at(1)).toInt();
                 //qDebug() << "LoTWUtilities::parseDownloadedFile: QSOs: " << QString::number(numQSO) << endl;
            }
            else if (lineU.contains("<I>USERNAME/PASSWORD INCORRECT</I>"))
            {
                userPasswordError = true;
            }
        }
        // WE HAVE JUST FINISHED TO READ THE HEADER OR THE FILE, IF IT IS NOT AN ADIF
        if (!hasHeader || (numQSO<1))
        {

             //qDebug() << "LoTWUtilities::parseDownloadedFile Header not found" << _fileName << endl;
            QString aux;
            if (userPasswordError)
            {
                msgBox.setWindowTitle(tr("KLog - LoTW user/password error"));
                msgBox.setText(tr("LoTW server did not recognized your user/password"));
                aux = QString(tr("Check your user and password and ensure your are using the right one before trying again.") );
            }
            else if(numQSO<1)
            {
                msgBox.setWindowTitle(tr("KLog - LoTW No QSOs "));
                msgBox.setText(tr("LoTW sent no QSOs"));
                aux = QString(tr("It seems that LoTW has no QSO with the Station Callsign you are using (%1).") ).arg(stationCallsign);
            }
            else
            {
                msgBox.setWindowTitle(tr("KLog - LoTW Unknown error"));
                msgBox.setText(tr("KLog can't recognize the file that has been downloaded from LoTW."));
                aux = QString(tr("Try again and send the downloaded file (%1) to the KLog developer for analysis.") ).arg(_fileName);
            }

            msgBox.setIcon(QMessageBox::Warning);

            msgBox.setDetailedText(aux);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setDefaultButton(QMessageBox::Ok);
            msgBox.exec();
            //qDebug() << "LoTWUtilities::parseDownloadedFile: END-2" << endl;
            file.remove();
            return ;
        }

        msgBox.setIcon(QMessageBox::Information);
        msgBox.setWindowTitle(tr("KLog - LoTW download"));
        msgBox.setText(tr("KLog downloaded %1 QSOs successfully. Do you want to update your log with the downloaded data?").arg(QString::number(numQSO)));
        aux = QString(tr("Now KLog will process the downloaded QSO and update your local log.") );

        msgBox.setDetailedText(aux);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);
        if (msgBox.exec() == QMessageBox::No)
        {
            //qDebug() << "LoTWUtilities::parseDownloadedFile: END-2" << endl;
            return ;
        }
        //file.seek(startOfFile);
        emit actionProcessLoTWDownloadedFile(_fileName);

    }


   //Procesar los QSOs y meterlos en una tabla? o en un QStringList o alguna otra estructura


     //qDebug() << "LoTWUtilities::parseDownloadedFile - END"  << endl;
}

void LoTWUtilities::showMessage(const int _messageIcon, const QString &_msg, const QString &_msgExt)
{

}
