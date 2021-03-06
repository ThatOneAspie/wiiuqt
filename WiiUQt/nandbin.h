#ifndef NANDBIN_H
#define NANDBIN_H

#include "includes.h"
#include "blocks0to1.h"
#include "nandspare.h"

enum dump_type_t
{
    NAND_DUMP_INVALID=0,
    NAND_DUMP_NO_ECC,
    NAND_DUMP_ECC,
    NAND_DUMP_BOOT_MII,
};

enum nand_type_t
{
    NAND_VWII = 0,
    NAND_WIIU,
};

struct fst_t
{
    quint8 filename[ 0xc ];
    quint8 attr;
    quint8 wtf;
    quint16 sub;
    quint16 sib;
    quint32 size;
    quint32 uid;
    quint16 gid;
    quint32 x3;
    quint16 fst_pos;//not really part of the nand structure, but needed when calculating hmac data
};
// class to deal with an encrypted wii nand dump
// basic usage... create an object, set a path, call InitNand.  then you can get the detailed list of entries with GetTree()
// extract files with GetFile()
//! you should verify anything written with this code before attempting to install it on you wii

//once InitNand() is called, you can get the contents of the nand in a nice QTreeWidgetItem* with GetTree()
class NandBin : public QObject
{
    Q_OBJECT

public:
    //creates a NandBin object. if a path is given, it will call SetPath() on that path.  though you cant check the return value
    NandBin( QObject * parent = 0, const QString &path = QString(), nand_type_t type = NAND_WIIU );

    //destroys this object, and all its used resources ( closes the nand.bin file and deletes the filetree )
    ~NandBin();

    //create a "blank" nand at the given path with spare data
    //bootBlocks should be a bytearray containing 0x42000 bytes - the first 16 clusters of the nand with spare data
    //badBlocks is a list of blocks to be marked bad, in the range 2/8 - 4079
    bool CreateNewVWii( const QString &path, const QList<quint16> &badBlocks);
    bool CreateNewWiiU( const QString &path, const QByteArray &bootBlocks, const QList<quint16> &badBlocks );

    //sets the path of this object to path.  returns false if it cannot open an already existing file
    //keys.bin should be in this same path if they are to be used
    bool SetPath( const QString &path );

    //try to read the filesystem and create a tree from its contents
    //this takes care of the stuff like reading the keys, finding teh superblock, and creating the QTreeWidgetItem* tree
    //icons given here will be the ones used when asking for that tree
    bool InitNand( const QIcon &dirs = QIcon(), const QIcon &files = QIcon() );

    //get a root item containing children that are actually entries in the nand dump
    //the root itself is just a container to hold them all and can be deleted once its children are taken
    //! all returned items are cloned and it is up to you to delete them !
    //text is assigned to the items as follows...
    // 0 name
    // 1 entry #
    // 2 size
    // 3 uid
    // 4 gid
    // 5 x3
    // 6 mode
    // 7 attr
    QTreeWidgetItem *GetTree();

    //extracts an item( and all its children ) to a directory
    //this function is BLOCKING and will block the current thread, so if done in the gui thread, it will freeze your GUI till it returns
    bool ExtractToDir( QTreeWidgetItem *item, const QString &path );

    //print a little info about the free space
    void ShowInfo();

    //set this to change ":" in names to "-" on etracting.
    //theres more illegal characters in FAT, but thes seems to be the only one that happens on the nand FS
    void SetFixNamesForFAT( bool fix = true );

    //returns the data that makes up the file of a given entry#
    const QByteArray GetFile( quint16 entry );


    //get data for a given path
    //! this function is slower than the above one, as it first iterates through the QTreeWidgetItems till it finds the right ono
    //! and then end up calling the above one anyways.
    //the path should be a file, not folder
    //returns an empty array on failure
    //path should start with "/" and items should be delimited by "/"
    //ie...  /title/00000001/00000002/data/setting.txt
    const QByteArray GetData( const QString &path );

    //returns the fats for this nand.
    const QList<quint16> GetFats() { return fats; }

    //get the fats for a given file
    const QList<quint16> GetFatsForFile( quint16 i );

    //recurse folders and files and get all fats used for them
    //! this is probably a more expensive function than you want to use
    //! it was added only to aid in checking for bugs and lost clusters
    const QList<quint16> GetFatsForEntry( quint16 i );

    //use the above function to search and display lost clusters
    void ShowLostClusters();

    const Blocks0to1 BootBlocks(){ return bootBlocks; }
    bool CheckBoot1();

    dump_type_t DumpType() { return dumpType; }
    nand_type_t NandType() { return nandType; }

    const QByteArray GetPage( quint32 pageNo, bool withEcc = false );

    //create new entry
    //returns the index of the entry on success, or 0 on error
    quint16 CreateEntry(  const QString &path, quint32 uid, quint16 gid, quint8 attr, quint8 user_perm, quint8 group_perm, quint8 other_perm );

    //delete a file/folder
    bool Delete( const QString &path );

    //sets the data for a given file ( overwrites existing data )
    bool SetData( quint16 idx, const QByteArray &data );

    //overloads the above function
    bool SetData( const QString &path, const QByteArray &data );

    //write the current changes to the metadata( if you dont do this, then none of the other stuff youve done wont be saved )
    // but at the same time, you probably dont need to overuse this.  ( no need to write metadata every time you make a single change )
    bool WriteMetaData();

    //functions to verify and fix spare data
    bool CheckEcc( quint32 pageNo );
    // warning: this clear the spare data (including the hmac)
    bool FixEcc( quint32 pageNo );
    bool CheckHmacData( quint16 entry );
    bool FixHmacData( quint16 entry );

    //verify hmac stuff for a given supercluster
    //expects 0x7f00 - 0x7ff0
    bool CheckHmacMeta( quint16 clNo );
    bool FixHmacMeta( quint16 clNo );

    //wipe out all data within the nand FS, leaving only the root entry
    //preserve all bad/reserved clusters
    //if secure is true, overwrite old file data with 0xff
    bool Format( bool secure = true );

    //get the path of this nand
    const QString FilePath();

    //get the keys.bin for this object
    const QByteArray Keys();

    qint32 GetFirstSuperblockCluster();

private:
    QByteArray key;
    qint32 loc_super;
    qint32 loc_fat;
    qint32 loc_fst;
    quint16 currentSuperCluster;
    quint32 superClusterVersion;
    QString extractPath;
    QString nandPath;
    QFile f;
    dump_type_t dumpType;
    nand_type_t nandType;

    static const qint32 PAGE_SIZE = 0x800;
    static const qint32 SPARE_SIZE = 0x40;
    static const quint16 CLUSTERS_COUNT = 0x8000;

    bool fatNames;
    QIcon groupIcon;
    QIcon keyIcon;

    NandSpare spare;//used to handle the hmac mumbojumbo

    //read all the fst and remember them rather than seeking back and forth in the file all the time
    // uses ~120KiB RAM
    bool fstInited;
    fst_t fsts[ 0x17ff ];

    //cache the fat to keep from having to look them up repeatedly
    // uses ~64KiB
    QList<quint16>fats;

    bool CreateNew( const QString &path, const QByteArray &bootBlocks, const QList<quint16> &badBlocks = QList<quint16>() );

    bool GetDumpType();
    bool GetNandType();
    bool GetKey();

    QString FindFile( const QString & name );
    qint32 GetPageSize();
    qint32 GetClusterSize();
    quint16 GetReservedClustersCount();
    const QByteArray ReadKeyfile( const QString &path, quint8 type );//type 0 for nand key, type 1 for hmac
    const QByteArray ReadOTPfile( const QString &path, quint8 type );//type 0 for nand key, type 1 for hmac
    qint32 FindSuperblock();
    quint16 GetFAT( quint16 fat_entry );
    fst_t GetFST( quint16 entry );
    const QByteArray GetCluster( quint16 cluster_entry, bool decrypt = true );
    const QByteArray GetFile( fst_t fst );

    const QString FstName( fst_t fst );
    bool ExtractFST( quint16 entry, const QString &path, bool singleFile = false );
    bool ExtractDir( fst_t fst, const QString &parent );
    bool ExtractFile( fst_t fst, const QString &parent );

    QTreeWidgetItem *CreateItem( QTreeWidgetItem *parent, const QString &name, quint32 size, quint16 entry, quint32 uid, quint32 gid, quint32 x3, quint8 attr, quint8 wtf);



    QTreeWidgetItem *root;
    bool AddChildren( QTreeWidgetItem *parent, quint16 entry );
    QTreeWidgetItem *ItemFromPath( const QString &path );
    QTreeWidgetItem *FindItem( const QString &s, QTreeWidgetItem *parent );

    //holds info about boot1
    Blocks0to1 bootBlocks;

    bool WriteCluster( quint32 pageNo, const QByteArray &data, const QByteArray &hmac );
    bool WriteDecryptedCluster( quint32 pageNo, const QByteArray &data, fst_t fst, quint16 idx );
    bool WritePage( quint32 pageNo, const QByteArray &data );
    bool WritePageSpare( quint32 pageNo, const QByteArray &data );

    quint16 CreateNode( const QString &name, quint32 uid, quint16 gid, quint8 attr, quint8 user_perm, quint8 group_perm, quint8 other_perm );

    bool DeleteItem( QTreeWidgetItem *item );
    //find a parent entry for a path to be created - "/title/00000001" should give the entry for "/title"
    QTreeWidgetItem *GetParent( const QString &path );

    QTreeWidgetItem *ItemFromEntry( quint16 i, QTreeWidgetItem *parent = NULL );
    QTreeWidgetItem *ItemFromEntry( const QString &i, QTreeWidgetItem *parent = NULL );

signals:
    //connect to these to receive messages from this object
    //so far, many errors are only outputting to qDebug()  and qWarning().
    void SendError( QString );
    void SendText( QString );
};

#endif // NANDBIN_H
