#ifndef DBCPARSER_H
#define DBCPARSER_H

#include <QStringList>
#include <QVector>
#ifdef HAVE_DBC_PARSER
#include <libdbc/dbc.hpp>
#endif

class QString;

class DbcParser {
public:
	enum class ParseStatus {
		Success,
		ErrorInvalidFile,
		ErrorBigEndian,
		ErrorMessageToLong,
		ErrorDBCParserUnsupported,
		ErrorUnknownID,
		ErrorInvalidConversion,
	};

	bool isValid();
	bool parseFile(const QString& filename);

	ParseStatus parseMessage(const uint32_t id, const std::vector<uint8_t>& data, std::vector<double>& out);

	struct ValueDescriptions {
		uint32_t value;
		QString description;
	};

	struct Signals {
		QStringList signal_names;
		std::vector<std::vector<ValueDescriptions>> value_descriptions;
	};

	/*!
	 * \brief numberSignals
	 * Determines the number of signals
	 * \param ids Vector with all id's found in a log file
	 * \return
	 */
	void signals(const QVector<uint32_t> ids, QHash<uint32_t, int>& idIndex, Signals& out);

private:
	bool m_valid{false};
	// QMap<uint32_t, libdbc::Message> m_messages;
#ifdef HAVE_DBC_PARSER
	libdbc::DbcParser m_parser;
#endif
};

#endif // DBCPARSER_H
