<?php
/**
 * @file
 * Helper functions.
 */

/**
 * Ingests the default thumbnail on an object.
 */
function utailla_set_default_thumb(AbstractObject $object) {
  $islandora_path = drupal_get_path('module', 'islandora');
  $datastream = $object->constructDatastream('TN', 'M');
  $datastream->label = 'Thumbnail';
  $datastream->mimetype = 'image/png';
  $datastream->setContentFromFile("$islandora_path/images/folder.png", FALSE);
  $object->ingestDatastream($datastream);
}

/**
 * Ingests the collection policy.
 */
function utailla_set_collection_policy(AbstractObject $object, $file_name) {
  $module_path = drupal_get_path('module', 'utailla');
  $collection_policy = $object->constructDatastream('COLLECTION_POLICY', 'M');
  $collection_policy->label = 'Collection Policy';
  $collection_policy->mimetype = 'application/xml';
  $collection_policy->setContentFromFile("$module_path/xml/$file_name", FALSE);
  $object->ingestDatastream($collection_policy);
}

/**
 * Ingests the DS-COMPOSITE.
 */
function utailla_set_ds_composite(AbstractObject $object, $file_name) {
  $module_path = drupal_get_path('module', 'utailla');
  $ds_composite = $object->constructDatastream('DS-COMPOSITE-MODEL', 'M');
  $ds_composite->label = 'DS-COMPOSITE-MODEL';
  $ds_composite->mimetype = 'application/xml';
  $ds_composite->setContentFromFile("$module_path/xml/$file_name", FALSE);
  $object->ingestDatastream($ds_composite);
}

/**
 * Recursively HTML decode and replace tags on a database row.
 *
 * The originating database had HTML fragments that we transform into text the
 * only tag was br so we are replacing these with newlines.
 */
function utailla_get_decoded_row_recursive($row_object) {
  $row_array = (array) $row_object;
  foreach ($row_array as $index => $column) {
    if (is_array($column)) {
      foreach ($column as $key => $value) {
        $row_array[$index][$key] = utailla_get_decoded_row_recursive($value);
      }
    }
    else {
      $row_array[$index] = preg_replace(
        '/<br\s*\/?>/i',
        "\n",
        html_entity_decode($column, ENT_QUOTES, 'UTF-8')
      );
    }
  }
  return $row_array;
}

/**
 * Restrict a datastream to the admin role.
 */
function utailla_restrict_datastream_to_admin($ds) {
  $xacml = new IslandoraXacml($ds->parent);
  $xacml->datastreamRule->addDsid($ds->id);
  $xacml->datastreamRule->addRole('admin');
  $xacml->writeBackToFedora();
}

/**
 * Get a site specific URI from a PID.
 */
function utailla_get_uri_from_pid($pid) {
  global $base_url;
  return "$base_url/islandora/object/$pid";
}

/**
 * Get a site specific URI from a user.
 */
function utailla_get_uri_from_user($uid) {
  global $base_url;
  return "$base_url/user/$uid";
}

/**
 * Get a MADS xml writer.
 */
function utailla_get_mads_writer() {
  $writer = new XMLWriter();
  $writer->openMemory();
  $writer->startDocument('1.0', 'utf-8');
  $writer->startElement('mads');
  $writer->writeAttribute('xmlns', 'http://www.loc.gov/mads/v2');
  $writer->writeAttribute('xmlns:xlink', 'http://www.w3.org/1999/xlink');
  $writer->writeAttribute('xmlns:xsi', 'http://www.w3.org/2001/XMLSchema-instance');
  $writer->writeAttribute('xmlns:mads', 'http://www.loc.gov/mads/v2');

  return $writer;
}

/**
 * Sets the user relations on an object based on the originating DB row.
 */
function utailla_set_user_rels(AbstractObject $object, $row) {
  // Link to user.
  if ($row['user_id']) {
    $uid = utailla_get_migrated_user_uid($row['user_id']);
    if ($uid) {
      $object->relationships->add(
        ISLANDORA_RELS_EXT_URI,
        UTAILLA_USER_REL,
        utailla_get_uri_from_user($uid)
      );
    }
  }
  // Link to contact user.
  if ($row['contact_id']) {
    $uid = utailla_get_migrated_contact_uid($row['contact_id']);
    if ($uid) {
      $object->relationships->add(
        ISLANDORA_RELS_EXT_URI,
        UTAILLA_CONTACT_REL,
        utailla_get_uri_from_user($uid)
      );
    }
  }
}

/**
 * Concatenate name parts based on a DB row.
 */
function utailla_concat_name_parts($db_row) {
  $name = '';
  $row = (object) $db_row;
  if ($row->first_name && !$row->last_name) {
    $name = $row->first_name;
  }
  elseif ($row->last_name && !$row->first_name) {
    $name = $row->last_name;
  }
  elseif ($row->last_name && $row->first_name) {
    $name = "{$row->first_name} {$row->last_name}";
  }
  return $name;
}

/**
 * Writes common MADS elements based on a db row.
 */
function utailla_write_common_mads($writer, $row) {
  // English description.
  if (trim($row['description_en'])) {
    $writer->startElement('note');
    $writer->writeAttribute('type', 'biographical/historical');
    $writer->writeAttribute('lang', 'en');
    $writer->text($row['description_en']);
    $writer->endElement();
  }
  // Spanish description.
  if (trim($row['description_sp'])) {
    $writer->startElement('note');
    $writer->writeAttribute('type', 'biographical/historical');
    $writer->writeAttribute('lang', 'sp');
    $writer->text($row['description_sp']);
    $writer->endElement();
  }
  // Depositor status.
  if ($row['depositor']) {
    $writer->startElement('note');
    $writer->writeAttribute('type', 'depositor status');
    if ($row['depositor'] == 'Y') {
      $writer->text('yes');
    }
    else {
      $writer->text('no');
    }
    $writer->endElement();
  }
}

/**
 * Writes a language MADS element.
 */
function utailla_write_mads_language($writer, $language_id, $type = 'research language') {
  $pid = utailla_migrated_language_pid($language_id);
  if ($pid) {
    $writer->startElement('fieldOfActivity');
    $writer->writeAttribute('type', $type);
    $writer->writeAttribute('authority', 'aillaLanguage');
    $writer->writeAttribute('valueURI', utailla_get_uri_from_pid($pid));
    $writer->endElement();
  }
}