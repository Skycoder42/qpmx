#include "searchcommand.h"
#include <iostream>
#include <qtcoroutine.h>
using namespace qpmx;

#define print(x) std::cout << QString(x).toStdString() << std::endl

SearchCommand::SearchCommand(QObject *parent) :
	Command{parent}
{}

QString SearchCommand::commandName() const
{
	return QStringLiteral("search");
}

QString SearchCommand::commandDescription() const
{
	return tr("Search for a package by it's name.");
}

QSharedPointer<QCliNode> SearchCommand::createCliNode() const
{
	auto searchNode = QSharedPointer<QCliLeaf>::create();
	searchNode->addOption({
							  {QStringLiteral("p"), QStringLiteral("provider")},
							  tr("Specify the <provider> to search for this package. Can be specified "
								 "multiple times to search multiple providers. If not specified, all "
								 "providers that support searching are searched."),
							  tr("provider")
						  });
	searchNode->addOption({
							  QStringLiteral("short"),
							  QStringLiteral("Only list package names (with provider) as space seperated list, no category based listing.")
						  });
	searchNode->addPositionalArgument(QStringLiteral("query"),
									  tr("The query to search by. Typically, a \"contains\" search is "
										 "performed, but some providers may support wildcard or regex expressions. "
										 "If you can't find a package, try a different search pattern first."));
	return searchNode;
}

void SearchCommand::initialize(QCliParser &parser)
{
	try {
		_short = parser.isSet(QStringLiteral("short"));

		if(parser.positionalArguments().isEmpty())
			throw tr("You must specify a search query to perform a search");
		auto query = parser.positionalArguments().join(QLatin1Char(' '));

		auto providers = parser.values(QStringLiteral("provider"));
		if(!providers.isEmpty()) {
			for(const auto &provider : providers) {
				if(!registry()->sourcePlugin(provider)->canSearch(provider))
					throw tr("Provider %{bld}%1%{end} does not support searching").arg(provider);
			}
		} else {
			for(const auto &provider : registry()->providerNames()) {
				if(registry()->sourcePlugin(provider)->canSearch(provider))
					providers.append(provider);
			}
			xDebug() << tr("Searching providers: %1").arg(providers.join(tr(", ")));
		}

		performSearch(query, providers);
	} catch(QString &s) {
		xCritical() << s;
	}
}

void SearchCommand::performSearch(const QString &query, const QStringList &providers)
{
	QtCoroutine::awaitEach(providers, [this, query](const QString &provider){
		try {
			auto plg = registry()->sourcePlugin(provider);
			auto packageNames = plg->searchPackage(provider, query);
			xDebug() << tr("Found %n result(s) for provider %{bld}%1%{end}", "", packageNames.size()).arg(provider);
			if(!packageNames.isEmpty())
				_searchResults.append({provider, packageNames});
		} catch(qpmx::SourcePluginException &e) {
			throw tr("Failed to search provider %{bld}%1%{end} with error: %2")
					.arg(provider, e.qWhat());
		}
	});

	printResult();
}

void SearchCommand::printResult()
{
	if(_short) {
		QStringList resList;
		for(const auto &res : qAsConst(_searchResults)) {
			for(const auto &pkg : res.second)
				resList.append(PackageInfo(res.first, pkg).toString(false));
		}
		print(resList.join(QLatin1Char(' ')));
	} else {
		for(const auto &res : qAsConst(_searchResults)) {
			print(tr("--- %1").arg(res.first + QLatin1Char(' '), -76, QLatin1Char('-')));
			for(const auto &pkg : res.second)
				print(tr(" %1").arg(pkg));
		}
	}

	qApp->quit();
}
