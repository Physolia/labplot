add_subdirectory(ASCII)
add_subdirectory(Binary)
add_subdirectory(JSON)
add_subdirectory(MQTT)
add_subdirectory(Project)
add_subdirectory(Spice)
add_subdirectory(CAN)

IF (ZLIB_FOUND AND LZ4_FOUND)
        add_subdirectory(ROOT)
ENDIF ()

IF (READSTAT_FOUND OR BUILD_READSTAT)
        add_subdirectory(ReadStat)
ENDIF ()

IF (MATIO_FOUND)
        add_subdirectory(Matio)
ENDIF ()
IF (HDF5_FOUND)
        add_subdirectory(HDF5)
ENDIF ()
IF (CFITSIO_FOUND)
        add_subdirectory(FITS)
ENDIF ()
IF (NETCDF_FOUND)
        add_subdirectory(NetCDF)
ENDIF ()
IF (QXLSX_FOUND OR BUILD_EXCEL)
        add_subdirectory(XLSX)
ENDIF ()
IF (ORCUS_FOUND)
        add_subdirectory(Ods)
ENDIF ()

add_subdirectory(DATASETS)
