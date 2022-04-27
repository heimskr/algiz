<!doctype html>
<html>
	<head>
		<title>Ansuz</title>
		<link rel="stylesheet" href="/ansuz/bootstrap.min.css" />
		<link rel="stylesheet" href="/ansuz/bootstrap-utilities.min.css" />
		<style>
			{{css}}
		</style>
	</head>
	<body>
		<header><a href="/ansuz">ᚨᚾᛊᚢᛉ</a></header>
		<main class="container">
			<table class="table text-white">
				<thead>
					<th scope="col">Plugin</th>
					<th scope="col" colspan="2">Version</th>
				</thead>
				<tbody>
				{% for plugin in plugins %}
					<tr>
						<td class="col-10" title="{{plugin.2}}">{{plugin.1}}</td>
						<td class="col-2">{{plugin.3}}</td>
						<td class="col-2"><a class="btn btn-primary" role="button" href="/ansuz/unload/{{plugin.0}}">Unload</a></td>
					</tr>
				{% endfor %}
				</tbody>
			</table>
		</main>
	</body>
</html>
